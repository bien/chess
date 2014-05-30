from chess import Board
import notation
import time
import random
import base64

class MoveException(Exception):
	pass
	
class QuitException(Exception):
	pass

class Human():
	def __init__(self, color):
		self.color = color
		self.algebra = notation.Algebra()
	
	def move(self, board):
		while True:
			text = raw_input("Your move> ")
			if text == "exit":
				raise QuitException
			try:
				return self.algebra.interpret(text, board, self.color)
			except notation.ParseException, e:
				print e
				
class Random():
	def __init__(self, color):
		self.color = color
		
	def move(self, board):
		return random.choice([x for x in board.legal_moves(self.color)])
			
class Minimax():
	def __init__(self, color, ply):
		self.color = color
		self.algebra = notation.Algebra()
		self.valuation = dict(r=-5,n=-3,b=-3,q=-9, p=-1, R=5, N=3, B=3, Q=9, P=1)
		self.ply = ply
		self.transposition_table = {}
		self.moveno = 1
		
	def board_evaluation(self, board, player_to_move):
		"""
		>>> b = Board()
		>>> m = Minimax('white', 3)
		>>> alg = notation.Algebra()
		>>> m.board_evaluation(b, 'white')
		0.0
		>>> b.set_square(1, 4, 'x')
		>>> m.board_evaluation(b, 'white')
		9.01
		>>> b.set_square(8, 1, 'x')
		>>> m.board_evaluation(b, 'black')
		4.01
		>>> b = Board()
		>>> x = b.apply_move(alg.interpret('a4', b, 'white'))
		>>> x = b.apply_move(alg.interpret('e5', b, 'black'))
		>>> x = b.apply_move(alg.interpret('Nh3', b, 'white'))
		>>> x = b.apply_move(alg.interpret('e4', b, 'black'))
		>>> x = b.apply_move(alg.interpret('Ng1', b, 'white'))
		>>> x = b.apply_move(alg.interpret('e3', b, 'black'))
		>>> x = b.apply_move(alg.interpret('dxe3', b, 'white'))
		>>> m.board_evaluation(b, 'black')
		1
		"""
		white_moves = [x for x in board.legal_moves_ignoring_check('white')]
		black_moves = [x for x in board.legal_moves_ignoring_check('black')]
		if player_to_move == 'white' and len(white_moves) == 0:
			if board.king_in_check(white):
				return -999
			else:
				return 0
		if player_to_move == 'black' and len(black_moves) == 0:
			if board.king_in_check(black):
				return 999
			else:
				return 0
		piece_values = [self.valuation.get(x, 0) for x in board.board]
		# draw due to lack of material
		if len([p for p in piece_values if p != 0]) == 0:
			return 0
		return sum(piece_values) + .01 * (len(white_moves) - len(black_moves))

	def with_move(self, board, move, callback):
		m = board.apply_move(move)
		result = callback(board)
		board.undo_move(m)
		return result
		
	def move(self, board):
		self.transposition_table = {}
		self.debug_graph = {}
		self.debug_score = {}
		self.debug_pruned = {}
		timer = time.time()
		score, move, chain, nodes = self.find_alphabeta(board, self.color, self.ply)
		elapsed = time.time() - timer
		print "alphabeta: move %d: %d nodes, chain=%s, score=%g, depth=%d, processed %g nodes/sec" % (self.moveno, nodes, ', '.join([board.format_move(m) for m in chain]), score, self.ply, nodes/(.0001+elapsed))
		self.debug_file = file('%d-%d.dot' % (self.moveno, self.ply), 'w')
		try:
			self.debug_file.write("""
	digraph move_graph {
		node [shape = circle];
	""")
			nodenames = {}
			for nodename, score in self.debug_score.iteritems():
				nodenames[nodename] = '%d (%g)' % (len(nodenames), score)
	#		nodenames = dict([(k, '%s (%g)' % (k, v)) for k, v in self.debug_score.iteritems()])
			for bm, result in self.debug_graph.iteritems():
				source, mm = bm
				self.debug_file.write('    "%s" -> "%s" [ label = "%s" ];\n' % (nodenames[source], nodenames[result], mm))
			for source, prunecount in self.debug_pruned.iteritems():
				self.debug_file.write('    "%s" -> "%s_pruned" [ label = "%d pruned" ];\n' % (nodenames[source], nodenames[source], prunecount))
			self.debug_file.write('}\n')
		finally:
			self.debug_file.close()
			self.debug_file = None
		return move
		
	def fast_eval(self, prev_board, move):
		return -self.valuation.get(board.get_square(m[2], m[3]), 0)
	
	def find_alphabeta(self, board, color, depth, alpha=-999999, beta=999999):
		if depth == 0:
			score = self.board_evaluation(board, color)
			self.debug_score[''.join(board.board)] = score
			return [score, None, [], 1]
		nodes = 1
		has_moves = False
		children = 0
		move_list = [x for x in board.legal_moves(color)]
		highest = [alpha, None, []]
		lowest = [beta, None, []]
		board_repr = ''.join(board.board)
		if depth > 1:
			move_list = sorted(move_list, key=lambda m: self.fast_eval(board, m), reverse=color == "white")
		for move in move_list:
			has_moves = True
			children += 1
			m = board.apply_move(move)
			self.debug_graph[(board_repr, board.format_move(move))] = ''.join(board.board)
			saved, chain, tdepth = self.transposition_table.get(board.compact_repr(), (None, [], 0))
			if saved and tdepth >= depth:
				candidate = [saved, move, [move] + chain]
			else:
				candidate = self.find_alphabeta(board, board.opposite_color(color), depth-1, highest[0], lowest[0])
				nodes += candidate[3]
				candidate = [candidate[0], move, [move] + candidate[2]]
			board.undo_move(m)
			if color == 'white':
				highest = max(highest, candidate)
				if highest[0] >= lowest[0]:
					self.debug_pruned[board_repr] = len(move_list) - children
					break # beta-cutoff
			else:
				if lowest[0] == candidate[0]:
					lowest = min(lowest, candidate, key=lambda a: -len(a[2]))
				else:
					lowest = min(lowest, candidate)
				if lowest[0] <= highest[0]:
					self.debug_pruned[board_repr] = len(move_list) - children
					break
		if not has_moves:
			score = self.board_evaluation(board, color)
			self.debug_score[''.join(board.board)] = score
			return [score, None, [], nodes]

		if color == 'white':
			self.transposition_table[board.compact_repr()] = (highest[0], highest[2], depth)
			self.debug_score[board_repr] = highest[0]
			return highest[0:3] + [nodes]
		else:
			self.transposition_table[board.compact_repr()] = (lowest[0], lowest[2], depth)
			self.debug_score[board_repr] = lowest[0]
			return lowest[0:3] + [nodes]
	
	def find_minimax(self, board, color, depth):
		if depth == 0:
			return [self.board_evaluation(board, color), None, [], 1]
		else:
			if color == 'white':
				selection = max
			else:
				selection = min
			candidates = []
			for move in board.legal_moves(color):
				m = board.apply_move(move)
				candidates.append(self.find_minimax(board, board.opposite_color(color), depth - 1))
				candidates[-1][1] = move
				board.undo_move(m)
			score, m, chain, nodes = selection(candidates)
			nodes = sum(c[3] for c in candidates) + 1
			chain.append(m)
			return [score, m, chain, nodes]

class IterativeDeepening(Minimax):
	def __init__(self, color, timelimit):
		Minimax.__init__(self, color, 3)
		self.timelimit = timelimit
	
	def move(self, board):
		start = time.time()
		self.ply = 1
		self.deepening_table = {}
		while time.time() - start < self.timelimit / 2:
			self.ply += 1
			best = Minimax.move(self, board)
			self.deepening_table = self.transposition_table
		self.moveno += 1
		return best
		
	def fast_eval(self, board, move):
		captured_piece = -self.valuation.get(board.get_square(move[2], move[3]), 0)
		m = board.apply_move(move)
		score = self.deepening_table.get(board.compact_repr(), self.transposition_table.get(board.compact_repr(), captured_piece))
		board.undo_move(m)
		return score

def makeplayer(spec, color):
	if spec == 'human':
		return Human(color)
	elif spec.startswith('minimax:'):
		return Minimax(color, int(spec.replace("minimax:", "")))
	elif spec.startswith('ai:'):
		return IterativeDeepening(color, int(spec.replace("ai:", "")))
	elif spec == 'random':
		return Random(color)
	else:
		return None

def test():
	import doctest
	doctest.testmod()

def main():
	import sys
	if len(sys.argv) < 3:
		print "Usage: %s (human|minimax:ply|ai:seconds-avg-per-move) [...]"
		sys.exit(1)
	board = Board()
	color = 'white'
	winner = None
	player = dict(white=makeplayer(sys.argv[1], 'white'), black=makeplayer(sys.argv[2], 'black'))
	while True:
		has_move = False
		for legal_move in board.legal_moves():
			has_move = True
			break
		if not has_move:
			if board.king_in_check(color):
				print "game over: %s wins" % board.opposite_color(color)
			else:
				print "game over: stalemate"
			break
		try:
			move = player[color].move(board)
		except QuitException:
			break
		print "%s played %s" % (color, board.format_move(move))
		board.apply_move(move)
		if color == 'black':
			print board.format()
		color = board.opposite_color(color)

if __name__ == '__main__':
	import sys
	if len(sys.argv) == 2 and sys.argv[1] == "test":
		test()
	else:
		main()
#test()
