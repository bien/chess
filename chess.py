
ChessSymbols=dict(R=u'\u2656', N=u'\u2658', B=u'\u2657', Q=u'\u2655', K=u'\u2654', P=u'\u2659', r=u'\u265c', k=u'\u265a', q=u'\u265b', b=u'\u265d', n=u'\u265e', p=u'\u265f', x='.')

class Board:
	def __init__(self):
		self.board = [c for c in 'RNBQKBNR' + ('P' * 8) + ('x' * 8 * 4) + ('p' * 8) + 'rnbqkbnr']
		self.can_castle = dict(white=[True, True], black=[True, True]) # kingside, queenside
		self.last_move = None
		self.turn = 'white'
		
	def format(self):
		return '\n'.join([''.join(c for c in self.board[i:i+8]) for i in reversed(range(0, 64, 8))])
		
	def get_square(self, rank, file):
		"""rank is in [1, 8] and file is [1, 8]
		>>> Board().get_square(1, 1)
		'R'
		>>> ''.join([Board().get_square(1, i) for i in range(1, 9)])
		'RNBQKBNR'
		>>> ''.join([Board().get_square(3, i) for i in range(1, 9)])
		'xxxxxxxx'
		>>> ''.join([Board().get_square(8, i) for i in range(1, 9)])
		'rnbqkbnr'
		"""
		assert rank in range(1, 9) and file in range(1, 9)
		return self.board[(rank - 1) * 8 + file - 1]

	def set_square(self, rank, file, piece):
		"""
		'rank is in [1, 8] and file is [1, 8]'
		>>> b = Board()
		>>> b.set_square(1, 1, 'x')
		>>> b.get_square(1, 1)
		'x'
		"""
		assert rank in range(1, 9) and file in range(1, 9)
		self.board[(rank - 1) * 8 + file - 1] = piece
		
	def get_piece(self, piece):
		"""
		>>> Board().get_piece('R')
		'r'
		>>> Board().get_piece('r')
		'r'
		"""
		if ord(piece) < ord('a'):
			return chr(ord(piece) + ord('a') - ord('A'))
		else:
			return piece
			
	def get_owner(self, piece):
		"""
		>>> Board().get_owner('R')
		'white'
		>>> Board().get_owner('r')
		'black'
		"""
		if ord(piece) < ord('a'):
			return 'white'
		elif piece == 'x':
			return 'empty'
		else:
			return 'black'
			
	def make_piece(self, piece, color):
		"""
		>>> Board().make_piece('r', 'white')
		'R'
		>>> Board().make_piece('p', 'black')
		'p'
		"""
		assert color in ('white', 'black')
		if color == 'white':
			return chr(ord(self.get_piece(piece)) + ord('A') - ord('a'))
		else:
			return self.get_piece(piece)
			
	def format_move(self, move):
		sr, sf, tr, tf = move[0:4]
		return '%s%d-%s%d%s' % (chr(sf + ord('a') - 1), sr, chr(tf + ord('a') - 1), tr, '=%s' % move[4] if len(move) == 5 else '') 
			
	def opposite_color(self, piece):
		"""
		>>> Board().opposite_color('r')
		'R'
		>>> Board().opposite_color('P')
		'p'
		"""
		if piece == 'white':
			return 'black'
		elif piece == 'black':
			return 'white'
		elif self.get_owner(piece) == 'white':
			return self.make_piece(piece, 'black')
		else:
			return self.make_piece(piece, 'white')
			
	def apply_direction(self, vector, magnitude, base_rank, base_file):
		"""
		>>> Board().apply_direction([1, 1], 3, 1, 1)
		(4, 4)
		"""
		return base_rank + vector[0] * magnitude, base_file + vector[1] * magnitude
		
	def apply_direction_to_edge(self, vector, base_rank, base_file, owner, limit=7):
		"""
		>>> b = Board()
		>>> b.set_square(2, 1, 'x')
		>>> [x for x in b.apply_direction_to_edge([1, 0], 1, 1, 'white')]
		[(2, 1), (3, 1), (4, 1), (5, 1), (6, 1), (7, 1)]
		"""
		for magnitude in range(1, limit+1):
			target_rank, target_file = self.apply_direction(vector, magnitude, base_rank, base_file)
			if target_rank not in range(1, 9) or target_file not in range(1, 9) or self.get_owner(self.get_square(target_rank, target_file)) == owner:
				break
			elif self.get_square(target_rank, target_file) == 'x':
				yield target_rank, target_file
			else:
				# opposite color piece
				yield target_rank, target_file
				break
				
	def apply_move(self, move):
		"""
		return a move that can be used for undo
		>>> b = Board()
		>>> b.apply_move((2, 1, 4, 1))
		((2, 1, 4, 1), 'x', None, {'white': [True, True], 'black': [True, True]})
		>>> b.get_square(2, 1)
		'x'
		>>> b.get_square(3, 1)
		'x'
		>>> b.get_square(4, 1)
		'P'
		>>> m = b.apply_move((7, 4, 5, 4))
		>>> m = b.apply_move((4, 1, 5, 1))
		>>> m = b.apply_move((7, 2, 5, 2))
		>>> m = b.apply_move((5, 1, 6, 2))
		>>> b.get_square(6, 2)
		'P'
		>>> b.get_square(5, 2)
		'x'
		>>> b.get_square(5, 1)
		'x'
		"""
		old_piece = self.get_square(move[2], move[3])
		if len(move) == 5:
			new_piece = self.make_piece(move[4], self.get_owner(self.get_square(move[0], move[1])))
		else:
			new_piece = self.get_square(move[0], move[1])
		self.set_square(move[2], move[3], new_piece)
		self.set_square(move[0], move[1], 'x')

		if move[2] in (1, 8) and move[3] == 7 and move[1] == 5 and self.get_piece(new_piece) == 'k':
			# king-side castle
			self.set_square(move[2], 6, self.get_square(move[2], 8))
			self.set_square(move[2], 8, 'x')
		elif move[2] in (1, 8) and move[3] == 3 and move[1] == 5 and self.get_piece(new_piece) == 'k':
			# queen-side castle
			self.set_square(move[2], 4, self.get_square(move[2], 1))
			self.set_square(move[2], 1, 'x')
		elif self.get_piece(new_piece) == 'p' and old_piece == 'x' and move[3] != move[1] and self.get_piece(self.get_square(move[0], move[3])) == 'p':
			# en-passant capture
			self.set_square(move[0], move[3], 'x')
			
		previous_castle = dict(white=self.can_castle['white'][:], black=self.can_castle['black'][:])
		if self.get_piece(new_piece) == 'k':
			self.can_castle[self.get_owner(new_piece)] = [False, False]
		elif self.get_piece(new_piece) == 'r' and move[1] == 1 and move[0] == 1:
			self.can_castle[self.get_owner(new_piece)][1] = False
		elif self.get_piece(new_piece) == 'r' and move[1] == 8 and move[0] == 1:
			self.can_castle[self.get_owner(new_piece)][0] = False
			
			
		previous_move = self.last_move
		self.last_move = move
			
		return move, old_piece, previous_move, previous_castle
		
	def undo_move(self, move_application):
		"""
		return a move that can be used for undo
		>>> b = Board()
		>>> ''.join(b.board)
		'RNBQKBNRPPPPPPPPxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxpppppppprnbqkbnr'
		>>> move = b.apply_move((2, 1, 4, 1))
		>>> ''.join(b.board)
		'RNBQKBNRxPPPPPPPxxxxxxxxPxxxxxxxxxxxxxxxxxxxxxxxpppppppprnbqkbnr'
		>>> move2 = b.apply_move((7, 1, 5, 1))
		>>> ''.join(b.board)
		'RNBQKBNRxPPPPPPPxxxxxxxxPxxxxxxxpxxxxxxxxxxxxxxxxppppppprnbqkbnr'
		>>> move_coords, captured_piece, previous_move, previous_castle = move2
		>>> b.get_square(move_coords[2], move_coords[3])
		'p'
		>>> b.undo_move(move2)
		>>> ''.join(b.board)
		'RNBQKBNRxPPPPPPPxxxxxxxxPxxxxxxxxxxxxxxxxxxxxxxxpppppppprnbqkbnr'
		>>> b.undo_move(move)
		>>> ''.join(b.board)
		'RNBQKBNRPPPPPPPPxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxpppppppprnbqkbnr'
		>>> move = b.apply_move((2, 4, 4, 4))
		>>> ''.join(b.board)
		'RNBQKBNRPPPxPPPPxxxxxxxxxxxPxxxxxxxxxxxxxxxxxxxxpppppppprnbqkbnr'
		>>> b.undo_move(move)
		>>> ''.join(b.board)
		'RNBQKBNRPPPPPPPPxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxpppppppprnbqkbnr'
		>>> b = Board()
		>>> m = b.apply_move((2, 1, 4, 1))
		>>> m = b.apply_move((7, 4, 5, 4))
		>>> m = b.apply_move((4, 1, 5, 1))
		>>> m = b.apply_move((7, 2, 5, 2))
		>>> state = ''.join(b.board)
		>>> m = b.apply_move((5, 1, 6, 2))
		>>> b.undo_move(m)
		>>> state == ''.join(b.board)
		True

		"""
		move, captured_piece, previous_move, previous_castle = move_application
		new_piece = self.get_square(move[2], move[3])
		if len(move) == 5:
			# promotion
			self.set_square(move[0], move[1], self.make_piece('p', self.get_owner(new_piece)))
		else:
			self.set_square(move[0], move[1], new_piece)
		self.set_square(move[2], move[3], captured_piece)
		self.last_move = previous_move
		self.can_castle = previous_castle

		if move[2] in (1, 8) and move[3] == 7 and move[1] == 5 and self.get_piece(new_piece) == 'k':
			# king-side castle
			self.set_square(move[2], 8, self.get_square(move[2], 6))
			self.set_square(move[2], 6, 'x')
		elif move[2] in (1, 8) and move[3] == 3 and move[1] == 5 and self.get_piece(new_piece) == 'k':
			# queen-side castle
			self.set_square(move[2], 1, self.get_square(move[2], 4))
			self.set_square(move[2], 4, 'x')
		elif self.get_piece(new_piece) == 'p' and captured_piece == 'x' and move[3] != move[1] and self.get_square(move[0], move[3]) == 'x':
			# en-passant capture
			self.set_square(move[0], move[3], self.opposite_color(new_piece))
		
	def king_in_check(self, player):
		"""
		>>> b = Board()
		>>> b.king_in_check("white")
		False
		>>> b.king_in_check("black")
		False
		>>> b.board = "xxxkBxxxxxxxxxxxxxxKxxxxxxxxxxxxxxxxxPbPxxxxxxxpxxxxxxxxxxxxxxxx"
		>>> b.board = "xxxxkxxxxxxxxxxxxxxKxxxxxxxxxxxxxxxxxPbPxxxxxxxpxxxxxxxxxxxxxxxx"
		>>> b.king_in_check("black")
		False
		"""
		otherplayer = 'black' if player == 'white' else 'white'
		king = None
		for rank in range(1, 9):
			for file in range(1, 9):
				if self.get_square(rank, file) == self.make_piece('k', player):
					king = (rank, file)
					break
			if king is not None:
				break

		if king is None:
			raise Exception, "Can't find king in %s" % ''.join(self.board)

		for vector in ([1, 0], [0, 1], [-1, 0], [0, -1]):
			for r, f in self.apply_direction_to_edge(vector, king[0], king[1], player):
				piece = self.get_square(r, f)
				if self.get_owner(piece) == otherplayer and self.get_piece(piece) in ('r', 'q'):
					return True
		for vector in ([1, 1], [-1, 1], [1, -1], [-1, -1]):
			for r, f in self.apply_direction_to_edge(vector, king[0], king[1], player):
				piece = self.get_square(r, f)
				if self.get_owner(piece) == otherplayer and self.get_piece(piece) in ('b', 'q'):
					return True
		for vector in ([1, 2], [2, 1], [1, -2], [2, -1], [-2, -1], [-1, -2], [-1, 2], [-2, 1]):
			for r, f in self.apply_direction_to_edge(vector, king[0], king[1], player, limit=1):
				piece = self.get_square(r, f)
				if self.get_owner(piece) == otherplayer and self.get_piece(piece) == 'n':
					return True
		direction = -1 if otherplayer == 'white' else 1
		for vector in ([direction, 1], [direction, -1]):
			for r, f in self.apply_direction_to_edge(vector, king[0], king[1], player, limit=1):
				piece = self.get_square(r, f)
				if self.get_owner(piece) == otherplayer and self.get_piece(piece) == 'p':
					return True
		for vector in ([1, 0], [0, 1], [-1, 0], [0, -1], [1, 1], [-1, 1], [1, -1], [-1, -1]):
			for r, f in self.apply_direction_to_edge(vector, rank, file, player, limit=1):
				piece = self.get_square(r, f)
				if self.get_owner(piece) == otherplayer and self.get_piece(piece) == 'k':
					return True
		return False
		
	def legal_moves(self, player=None):
		"""
		>>> b = Board()
		>>> [b.format_move(x) for x in b.legal_moves("white")]
		['b1-c3', 'b1-a3', 'g1-h3', 'g1-f3', 'a2-a3', 'a2-a4', 'b2-b3', 'b2-b4', 'c2-c3', 'c2-c4', 'd2-d3', 'd2-d4', 'e2-e3', 'e2-e4', 'f2-f3', 'f2-f4', 'g2-g3', 'g2-g4', 'h2-h3', 'h2-h4']
		>>> m = b.apply_move((2, 6, 3, 6))
		>>> [b.format_move(x) for x in b.legal_moves("black")]
		['a7-a6', 'a7-a5', 'b7-b6', 'b7-b5', 'c7-c6', 'c7-c5', 'd7-d6', 'd7-d5', 'e7-e6', 'e7-e5', 'f7-f6', 'f7-f5', 'g7-g6', 'g7-g5', 'h7-h6', 'h7-h5', 'b8-a6', 'b8-c6', 'g8-f6', 'g8-h6']
		>>> m = b.apply_move((7, 5, 5, 5))
		>>> [b.format_move(x) for x in b.legal_moves("white")]
		['b1-c3', 'b1-a3', 'e1-f2', 'g1-h3', 'a2-a3', 'a2-a4', 'b2-b3', 'b2-b4', 'c2-c3', 'c2-c4', 'd2-d3', 'd2-d4', 'e2-e3', 'e2-e4', 'g2-g3', 'g2-g4', 'h2-h3', 'h2-h4', 'f3-f4']
		>>> m = b.apply_move((2, 7, 4, 7))
		>>> [b.format_move(x) for x in b.legal_moves("black")]
		['e5-e4', 'a7-a6', 'a7-a5', 'b7-b6', 'b7-b5', 'c7-c6', 'c7-c5', 'd7-d6', 'd7-d5', 'f7-f6', 'f7-f5', 'g7-g6', 'g7-g5', 'h7-h6', 'h7-h5', 'b8-a6', 'b8-c6', 'd8-e7', 'd8-f6', 'd8-g5', 'd8-h4', 'e8-e7', 'f8-e7', 'f8-d6', 'f8-c5', 'f8-b4', 'f8-a3', 'g8-f6', 'g8-e7', 'g8-h6']
		>>> m = b.apply_move((8, 4, 4, 8))
		>>> [b.format_move(x) for x in b.legal_moves("white")]
		[]
		>>> b = Board()
		>>> m = b.apply_move((2, 5, 4, 5))
		>>> m = b.apply_move((7, 4, 5, 4))
		>>> m = b.apply_move((2, 4, 4, 4))
		>>> [b.format_move(x) for x in b.legal_moves("black")]
		['d5-e4', 'a7-a6', 'a7-a5', 'b7-b6', 'b7-b5', 'c7-c6', 'c7-c5', 'e7-e6', 'e7-e5', 'f7-f6', 'f7-f5', 'g7-g6', 'g7-g5', 'h7-h6', 'h7-h5', 'b8-a6', 'b8-d7', 'b8-c6', 'c8-d7', 'c8-e6', 'c8-f5', 'c8-g4', 'c8-h3', 'd8-d7', 'd8-d6', 'e8-d7', 'g8-f6', 'g8-h6']
		>>> m = b.apply_move((8, 2, 6, 3))
		>>> m = b.apply_move((4, 5, 5, 5))
		>>> m = b.apply_move((7, 6, 5, 6))
		>>> [b.format_move(x) for x in b.legal_moves("white")]
		['b1-d2', 'b1-c3', 'b1-a3', 'c1-d2', 'c1-e3', 'c1-f4', 'c1-g5', 'c1-h6', 'd1-d2', 'd1-d3', 'd1-e2', 'd1-f3', 'd1-g4', 'd1-h5', 'e1-e2', 'e1-d2', 'f1-e2', 'f1-d3', 'f1-c4', 'f1-b5', 'f1-a6', 'g1-h3', 'g1-e2', 'g1-f3', 'a2-a3', 'a2-a4', 'b2-b3', 'b2-b4', 'c2-c3', 'c2-c4', 'f2-f3', 'f2-f4', 'g2-g3', 'g2-g4', 'h2-h3', 'h2-h4', 'e5-e6', 'e5-f6']
		>>> b = Board()
		>>> for i in (2, 3, 4, 6, 7): b.set_square(1, i, 'x')
		>>> for i in range(1, 9): b.set_square(2, i, 'x')
		>>> m = b.apply_move((1, 5, 1, 7))
		>>> m
		((1, 5, 1, 7), 'x', None, {'white': [True, True], 'black': [True, True]})
		>>> b.undo_move(m)
		>>> sorted([b.format_move(x) for x in b.legal_moves("white")])
		['a1-a2', 'a1-a3', 'a1-a4', 'a1-a5', 'a1-a6', 'a1-a7', 'a1-b1', 'a1-c1', 'a1-d1', 'e1-c1', 'e1-d1', 'e1-d2', 'e1-e2', 'e1-f1', 'e1-f2', 'e1-g1', 'h1-f1', 'h1-g1', 'h1-h2', 'h1-h3', 'h1-h4', 'h1-h5', 'h1-h6', 'h1-h7']
		"""
		if player is None:
			player = self.turn
		for move_notation in self.legal_moves_ignoring_check(player):
			move = self.apply_move(move_notation)
			in_check = self.king_in_check(player)
			self.undo_move(move)
			if not in_check:
				yield move_notation
		
	def legal_moves_ignoring_check(self, player=None):
		"""
		>>> b = Board()
		>>> [b.format_move(x) for x in b.legal_moves_ignoring_check("white")]
		['b1-c3', 'b1-a3', 'g1-h3', 'g1-f3', 'a2-a3', 'a2-a4', 'b2-b3', 'b2-b4', 'c2-c3', 'c2-c4', 'd2-d3', 'd2-d4', 'e2-e3', 'e2-e4', 'f2-f3', 'f2-f4', 'g2-g3', 'g2-g4', 'h2-h3', 'h2-h4']
		>>> ''.join(b.board)
		'RNBQKBNRPPPPPPPPxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxpppppppprnbqkbnr'
		>>> [b.format_move(x) for x in b.legal_moves_ignoring_check("black")]
		['a7-a6', 'a7-a5', 'b7-b6', 'b7-b5', 'c7-c6', 'c7-c5', 'd7-d6', 'd7-d5', 'e7-e6', 'e7-e5', 'f7-f6', 'f7-f5', 'g7-g6', 'g7-g5', 'h7-h6', 'h7-h5', 'b8-a6', 'b8-c6', 'g8-f6', 'g8-h6']
		>>> b.board = 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxPxxxxxxxx'
		>>> sorted([b.format_move(x) for x in b.legal_moves_ignoring_check("white")])
		['h7-h8=B', 'h7-h8=N', 'h7-h8=Q', 'h7-h8=R']
		"""
		if player is None:
			player = self.turn
		otherplayer = 'black' if player == 'white' else 'white'
		for rank in range(1, 9):
			for file in range(1, 9):
				square = self.get_square(rank, file)
				piece_type = self.get_piece(square)
				owner = self.get_owner(square)
				if owner != player or piece_type == 'x':
					continue
				if piece_type == 'r' or piece_type == 'q':
					for vector in ([1, 0], [0, 1], [-1, 0], [0, -1]):
						for r, f in self.apply_direction_to_edge(vector, rank, file, player):
							yield rank, file, r, f
				if piece_type == 'b' or piece_type == 'q':
					for vector in ([1, 1], [-1, 1], [1, -1], [-1, -1]):
						for r, f in self.apply_direction_to_edge(vector, rank, file, player):
							yield rank, file, r, f
				if piece_type == 'n':
					for vector in ([1, 2], [2, 1], [1, -2], [2, -1], [-2, -1], [-1, -2], [-1, 2], [-2, 1]):
						for r, f in self.apply_direction_to_edge(vector, rank, file, player, limit=1):
							yield rank, file, r, f
				if piece_type == 'k':
					for vector in ([1, 0], [0, 1], [-1, 0], [0, -1], [1, 1], [-1, 1], [1, -1], [-1, -1]):
						for r, f in self.apply_direction_to_edge(vector, rank, file, player, limit=1):
							yield rank, file, r, f

					backrank = (1 if owner == 'white' else 8)
					# castling kingside
					if rank == backrank and file == 5 and (self.can_castle['white'][0] if owner == 'white' else self.can_castle['black'][0]) and [self.get_square(backrank, i) for i in range(6, 8)] == ['x', 'x'] and self.get_square(backrank, 8) == self.make_piece('r', owner):
						yield rank, file, backrank, 7
					# castling queenside
#					print rank == backrank, file == 5, self.can_castle, (self.can_castle['white'][1] if owner == 'white' else self.can_castle['black'][1]), [self.get_square(backrank, i) for i in range(2, 5)] , self.get_square(backrank, 1) == self.make_piece('r', owner)
					if rank == backrank and file == 5 and (self.can_castle['white'][1] if owner == 'white' else self.can_castle['black'][1]) and [self.get_square(backrank, i) for i in range(2, 5)] == ['x', 'x', 'x'] and self.get_square(backrank, 1) == self.make_piece('r', owner):
						yield rank, file, backrank, 3
						
				if piece_type == 'p':
					direction = 1 if owner == 'white' else -1
					for r, f in self.apply_direction_to_edge([direction, 0], rank, file, player, limit=(2 if (rank == 2 and owner == 'white') or (rank == 7 and owner == 'black') else 1)):
						if self.get_square(r, f) == 'x':
							if r == 8 or r == 1:
								for promotion in 'RBNQ':
									yield rank, file, r, f, promotion
							else:
								yield rank, file, r, f
					# capture
					for filedelta in [1, -1]:
						for r, f in self.apply_direction_to_edge([direction, filedelta], rank, file, player, limit=1):
							if self.get_owner(self.get_square(r, f)) == otherplayer:
								if r == 8 or r == 1:
									for promotion in 'RBNQ':
										yield rank, file, r, f, promotion
								else:
									yield rank, file, r, f
					# en passant capture
					if rank == (5 if owner == 'white' else 4):
						for filedelta in [1, -1]:
							for r, f in self.apply_direction_to_edge([direction, filedelta], rank, file, player, limit=1):
#								print r, f, otherplayer, self.get_square(r, f), self.get_square(rank, f), self.make_piece('p', otherplayer), self.last_move

								if self.get_square(r, f) == 'x' and self.get_square(rank, f) == self.make_piece('p', otherplayer) and self.last_move == (2 if otherplayer == 'white' else 7, f, 4 if otherplayer == 'white' else 5, f):
									yield rank, file, r, f
				
if __name__ == '__main__':
	import doctest
	doctest.testmod()
