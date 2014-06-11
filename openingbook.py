import notation
import chess
import types

class Node:
	def __init__(self):
		self.white = 0
		self.black = 0
		self.moves = {}
		
	def add_move(self, move):
		if not move in self.moves:
			self.moves[move] = Node()
		return self.moves[move]
			
def compile_book(f):
	format = notation.Algebra()
	board = chess.Board()
	player = 'white'
	winner = None
	root = Node()
	node = None
	for move in notation.parse_pgn(f):
		if type(move) == types.TupleType and move[0] == "done":
			board = chess.Board()
			player = 'white'
			continue
		elif type(move) == types.TupleType and move[0] == 'metadata':
			winner = notation.Winner[move[1]['Result']]
			node = root
			continue
		parsed = format.interpret(move, board, player)
		node = node.add_move(parsed)
		if winner == 'white':
			node.white += 1
		elif winner == 'black':
			node.black += 1
		else:
			node.white += .5
			node.black += .5
		
		board.apply_move(parsed)
		if player == 'white':
			player = 'black'
		else:
			player = 'white'
	return root

def display_move(output, move, node, depth):
	if move is not None:
		output.write("%s%s score=%g/%g\n" % (' ' * depth, notation.format_move(move), node.white, node.black))
	for child_move, child_node in sorted(node.moves.iteritems(), key=lambda x: x[1].white + x[1].black, reverse=True):
		if child_node.white + child_node.black > 3 and depth < 10:
			display_move(output, child_move, child_node, depth + 1)

def display_book(f, output):
	root = compile_book(f)
	display_move(output, None, root, 0)

def openfile(filename):
	if filename.endswith('.zip'):
		import zipfile
		archive = zipfile.ZipFile(filename, 'r')
		for archivefile in archive.namelist():
			if archivefile.endswith('.pgn'):
				return archive.open(archivefile, 'r')
	elif filename.endswith('.pgn'):
		return file(filename)
	else:
		raise Exception
		
def main():
	import sys
	display_book(openfile(sys.argv[1]), sys.stdout)

main()
