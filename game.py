from chess import Board
import notation
import time

class MoveException(Exception):
	pass
	
class QuitException(Exception):
	pass

class Human():
	def __init__(self, color):
		self.color = color
		self.algebra = notation.Algebra()
	
	def move(self, board):
		print board.format()
		while True:
			text = raw_input("Your move> ")
			if text == "exit":
				raise QuitException
			return self.algebra.interpret(text, board, self.color)
			
class Minimax():
	def __init__(self, color, ply):
		self.color = color
		self.algebra = notation.Algebra()
		self.valuation = dict(r=5,n=3,b=3,q=9,k=999, p=1, R=-1, N=-3, B=-3, Q=-9, P=-1, K=-999)
		self.ply = ply
		
	def board_evaluation(self, board):
		return sum([sum([self.valuation.get(board.get_square(rank, file), 0) for rank in range(1, 9)]) for file in range(1, 9)])

	def with_move(self, board, move, callback):
		m = board.apply_move(move)
		result = callback(board)
		board.undo_move(m)
		return result
		
	def move(self, board):
		timer = time.time()
		score, move, chain, nodes = self.find_minimax(board, self.color, self.ply)
		elapsed = time.time() - timer
		print "minimax: %d nodes, chain=%s, score=%d, processed %g nodes/sec" % (nodes, ', '.join([board.format_move(m) for m in chain]), score, nodes/elapsed)
		timer = time.time()
		score, move, chain, nodes = self.find_alphabeta(board, self.color, self.ply)
		elapsed = time.time() - timer
		print "alphabeta: %d nodes, chain=%s, score=%d, processed %g nodes/sec" % (nodes, ', '.join([board.format_move(m) for m in chain]), score, nodes/elapsed)
		return move
		
	def find_alphabeta(self, board, color, depth, alpha=None, beta=None):
		if depth == 0:
			return [self.board_evaluation(board), None, [], 1]
		if alpha is None:
			alpha = [-999999, None, [], 1]
		if beta is None:
			beta = [999999, None, [], 1]
		nodes = 1
		for move in board.legal_moves(color):
			m = board.apply_move(move)
			candidate = self.find_alphabeta(board, board.opposite_color(color), depth-1, alpha, beta)
			candidate[1] = move
			candidate[2] = candidate[2] + [move]
			board.undo_move(m)
			nodes += candidate[3]
			if color == 'white':
				alpha = max(alpha, candidate)
				if alpha >= beta:
					break # beta-cutoff
			else:
				beta = min(beta, candidate)
				if beta <= alpha:
					break
		if color == 'white':
			return alpha[0:3] + [nodes]
		else:
			return beta[0:3] + [nodes]
	
	def find_minimax(self, board, color, depth):
		if depth == 0:
			return [self.board_evaluation(board), None, [], 1]
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

def main():
	import sys
	ply = 3
	if len(sys.argv) > 1:
		ply = int(sys.argv[1])
	board = Board()
	color = 'white'
	winner = None
	player = dict(white=Human('white'), black=Minimax('black', ply))
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
		color = board.opposite_color(color)

	
main()
