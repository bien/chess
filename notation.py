from chess import Board
import re
import types

class ParseException(Exception):
	pass

def read(f):
	format = Algebra()
	board = Board()
	player = 'white'
	for move in parse_pgn(f):
		if type(move) == types.TupleType and move[0] == "done":
			print "winner", move[1]
			board = Board()
			player = 'white'
			continue
		elif type(move) == types.TupleType and move[0] == 'metadata':
			print move[1]
			continue
		parsed = format.interpret(move, board, player)
		print '...' if player == 'black' else '', move, parsed
#		legal_moves = [x for x in board.legal_moves(player)]
#		if not parsed in legal_moves:
#			raise Exception, "%s is not legal: not in %s" % (repr(parsed), repr(sorted([board.format_move(m) for m in legal_moves])))
		board.apply_move(parsed)
		if player == 'white':
			player = 'black'
		else:
			player = 'white'

Winner = { '0-1': 'black', '1-0': 'white', '1/2-1/2': 'draw' }

def parse_pgn(f):
	meta = {}
	for line in f:
		m = re.match('\[(\w+) "(.*)"\]', line)
		if m:
			meta[m.groups(1)] = m.groups(2)
		elif line.strip():
			moves_found = 0
			for pseudomove in re.split('\d+\.', line):
				if not pseudomove.strip():
					continue
				parts = pseudomove.strip().split(' ')
				if len(parts) == 2:
					yield parts[0]
					yield parts[1]
				elif len(parts) == 4 and parts[2] == '' and parts[3] in Winner:
					yield parts[0]
					yield parts[1]
					yield "done", Winner[parts[3]]
				elif len(parts) == 3 and parts[1] == '' and parts[2] in Winner:
					yield parts[0]
					yield "done", Winner[parts[2]]
				else:
					raise ParseException, "Can't parse pseudomove %s %s" % (pseudomove, repr(parts))
				moves_found += 1
			if moves_found == 0:
				raise ParseException, "Can't read %s" % line
		elif meta:
			yield "metadata", meta
			meta = {}

class Notation:
	def interpret(self, move, board, player):
		pass
		
class Basic(Notation):
	def interpret(self, move, board, player):
		m = re.match('\s*([a-h])([1-8])[x\-]?([a-h])([1-8])\s*')
		if m:
			return ord(m.group(1)) - ord('a') + 1, int(m.group(2)), ord(m.group(3)) - ord('a') + 1, int(m.group(4))
		else:
			raise ParseException, "Can't parse %s" % move
			
class Algebra(Notation):
	def interpret(self, move, board, player):
		"""
		>>> b = Board()
		>>> Algebra().interpret('b8=Q', b, 'white')
		"""
		m = re.match('^(?P<piece>[KNBRQ]?)(?P<originrank>[1-8]?)(?P<originfile>[a-h]?)x?(?P<file>[a-h])(?P<rank>[1-8])(=(?P<promote>[BNRQ]))?(?P<check>\+?\+?)$', move)
		if m:
			piece = board.get_piece(m.group('piece') or 'p')
			src_file = ord(m.group('originfile')) - ord('a') + 1 if m.group('originfile') else None
			src_rank = int(m.group('originrank'))if m.group('originrank') else None
			dest_file = ord(m.group('file')) - ord('a') + 1
			dest_rank = int(m.group('rank'))
			promotion = m.group('promote')
			check = m.group('check')
#			print "Checking match for %s %s=(%s %s%d-%s%d)" % (player, move, piece, src_file or 'x', src_rank or 0, dest_file, dest_rank)
			for candidate in board.legal_moves(player):
				if dest_rank == candidate[2] and dest_file == candidate[3] and piece == board.get_piece(board.get_square(candidate[0], candidate[1])) and (src_rank is None or src_rank == candidate[0]) and (src_file is None or src_file == candidate[1]) and (len(candidate) == 4 or promotion == candidate[4]):
					if check == '+':
						m = board.apply_move(candidate)
						causes_check = board.king_in_check(board.opposite_color(player))
						board.undo_move(m)
						if causes_check:
							return candidate
					else:
						return candidate
			print board.format()
			raise ParseException, "Couldn't find match for %s %s=(%s %s%d-%s%d) among %s" % (player, move, piece, src_file or 'x', src_rank or 0, dest_file, dest_rank, repr(sorted([board.format_move(m) for m in board.legal_moves(player)])))
		elif move == 'O-O':
			if player == 'white':
				return (1, 5, 1, 7)
			else:
				return (8, 5, 8, 7)
		elif move == 'O-O-O':
			if player == 'white':
				return (1, 5, 1, 3)
			else:
				return (8, 5, 8, 3)
		else:
			raise ParseException, "Couldn't parse %s" % move

class Printer:
	def show(self, board):
		for rank in range(8, 0, -1):
			print "%d  %s" % (rank, board.board[(rank-1)*8:rank*8])
		print "   abcdefgh"

def test():
	import doctest
	doctest.testmod()

def main():
	import sys
	inf = sys.stdin
	if len(sys.argv) > 1:
		inf = file(sys.argv[1])
	read(inf)

if __name__ == '__main__':
	main()
