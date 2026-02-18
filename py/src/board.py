# Hive engine
import sys
import os
import random
import time

import collections

# clock-wise directions starting from East up to North-East
DIRECTIONS = [        
    (1, 0), (0, 1), (-1, 1), (-1, 0), (0, -1), (1, -1)
]

# UHP Direction to Symbol,Side
UHP_DIR_SYM = {
    (1, 0):  ("-", "r"),        # East
    (-1, 0): ("-", "l"),        # West
    (0, -1): ("\\", "l"),       # North-West
    (0, 1):  ("\\", "r"),       # South-East
    (-1, 1): ("/", "r"),        # South-West
    (1, -1): ("/", "l")         # North-East
}
# UHP Symbol,Side to Direction 
UHP_SYM_DIR : dict[tuple[str, str], tuple[int, int]] = {
    ("-", "r") : (1, 0),
    ("-", "l") : (-1, 0),
    ("\\", "r") : (0, 1),
    ("\\", "l") : (0, -1),
    ("/", "r") : (-1, 1),
    ("/", "l") : (1, -1)
}

# all the pieces in the game, for both players, with their respective counts
PIECES = [
    'wQ', 'wS1', 'wS2', 'wB1', 'wB2', 'wG1', 'wG2', 'wG3', 'wA1', 'wA2', 'wA3', 'wM', 'wL', 'wP',
    'bQ', 'bS1', 'bS2', 'bB1', 'bB2', 'bG1', 'bG2', 'bG3', 'bA1', 'bA2', 'bA3', 'bM', 'bL', 'bP'
]



class HiveBoard:
    """
    Class for easy manipulation of a Board of Hive game.
    """

    def __init__(self):
        self._reset()



    def _reset(self)->None:
        self.board : dict[tuple[int, int], list[str]] = {}

        self.pieces : dict = {}
        self.white_hand : list = ['wQ', 'wS1', 'wS2', 'wB1', 'wB2', 'wG1', 'wG2', 'wG3', 'wA1', 'wA2', 'wA3', 'wM', 'wL', 'wP']
        self.black_hand : list = ['bQ', 'bS1', 'bS2', 'bB1', 'bB2', 'bG1', 'bG2', 'bG3', 'bA1', 'bA2', 'bA3', 'bM', 'bL', 'bP']
        self.history : list[dict] = []

        self.turn : int = 1
        self.current_player : str = 'w'



    def parse_gamestring(self, gamestring : str)->str:
        """
        Parses the full game string from Mzinga and updates the board state accordingly.

        Parameters
        ----------
        gamestring: str
            The game string provided by Mzinga, expected to be in the format:  
            "Base;Turn=1;Move=0;Pieces=wQ[0,0],bQ[1,0],wS1[0,1],..." where pieces are listed as colorPiece[coor_q, coor_r].

        Returns
        -------
        int
            Returns 0 on successful parsing, or 1 if a ValueError is encountered during parsing.
        """

        self._reset()
        
        # Empty string
        if gamestring == "":
            self.current_player = 'w'
            return "Base+MLP;NotStarted;White[1]"
        
        chunks = gamestring.split(';')
        
        # GameTypeString: 
        if len(chunks) == 1:
            self.current_player = 'w'
            return "Base+MLP;NotStarted;White[1]"
        
        chunk_game_type, chunk_game_state, chunk_player_turn, chunk_moves = chunks[0], chunks[1], chunks[2], chunks[3:]
        assert chunk_game_type == 'Base+MLP', "Currently only 'Base+MLP' is supported"

        chunks_player_turn = chunk_player_turn.split("[")
        self.current_player = chunks_player_turn[0]
        self.turn = int(float(chunks_player_turn[1][:-1]))

        for move in chunk_moves:
            self.apply_move(move_string=move)
                    
        return "Base+MLP;InProgress;{}[{}];{}".format(chunks_player_turn[0], chunks_player_turn[1][:-1], ";".join(chunk_moves))


    def apply_move(self, move_string : str)->int:
        """
        Apply a move onto the Hive board. 
        Move should follow the UHP protocol:
            colorPiece symbolColorPiece
        or
            colorPiece colorPieceSymbol

        Parameters
        ----------
        move_string : str
            A string in the UHP protocol

        Returns
        -------
        int
            0, otherwise 1 if error
        """            
        chunks = move_string.split()

        # Pass move
        if move_string == "pass":
            self.turn += 1
            self.current_player = 'b' if self.current_player == 'w' else 'w'
            self.history.append({
                "move_type": "pass",
                "piece": None,
                "coords": None,
                "previous_coords": None
            })
            return 0
        
        piece = chunks[0] 
        hand = self.white_hand if piece.startswith('w') else self.black_hand 
        assert piece in PIECES, f"Invalid piece name: {piece}"


        # first move 
        if len(chunks) == 1:
            self._place_piece(piece_name=piece, piece_coord=(0, 0))
            return 0

        # symbol is any of  "-" , "\" or "/" in the second chunk
        symbol = ""
        for letter in chunks[1]:
            if letter in ["-", "\\", "/"]:
                symbol = letter
                break
        if chunks[1].startswith(symbol):
            ref_piece = chunks[1][1:]
            side = "l"
        else:
            ref_piece = chunks[1][:-1]
            side = "r"

        # remove piece from hand
        if piece in hand:
            hand.remove(piece)
        # remove piece from board
        else:
            old_q, old_r = self.pieces[piece]
            self.board[(old_q, old_r)].remove(piece)
            if len(self.board[(old_q, old_r)]) == 0:
                del (self.board[(old_q, old_r)])

        # place piece on the board, at the new coordinates
        ref_q, ref_r = self.pieces[ref_piece]
        dq, dr = UHP_SYM_DIR[(symbol, side)]
        piece_q, piece_r = ref_q + dq, ref_r + dr

        self._place_piece(piece_name=piece, piece_coord=(piece_q, piece_r))

        return 0



    # ------ UTILS ------ 
    def _place_piece(self, piece_name : str, piece_coord : tuple[int, int])->None:
        """
        Internal method for placing pieces inside the board.

        Parameters
        ----------
        piece_name : str
            Name of the piece to insert
        coordinates : tuple[int, int]
            Coordinates of the piece to insert
        """
        self.pieces[piece_name] = piece_coord
        self.board[piece_coord].append(piece_name) if piece_coord in self.board else self.board.__setitem__(piece_coord, [piece_name])
        # remove piece from hand
        if piece_name.startswith('w') and piece_name in self.white_hand:
            self.white_hand.remove(piece_name)
        elif piece_name.startswith('b') and piece_name in self.black_hand:
            self.black_hand.remove(piece_name)
        # expand history
        self.history.append({
            "move_type": "place",
            "piece": piece_name,
            "coords": piece_coord,
            "previous_coords": None
        })
        # update turn and player
        self.turn += 1
        self.current_player = 'b' if self.current_player == 'w' else 'w'
        return



    def _move_piece(self, piece_name : str, new_coords : tuple[int, int])->None:
        """
        Internal method for moving pieces inside the board.

        Parameters
        ----------
        piece_name : str
            Name of the piece to move
        new_coords : tuple[int, int]
            New coordinates of the piece to move
        """
        old_coords = self.pieces[piece_name]
        self.board[old_coords].remove(piece_name)
        if not self.board[old_coords]:  # If the stack is now empty, remove the key from the board
            del self.board[old_coords]

        self.pieces[piece_name] = new_coords
        if new_coords in self.board:
            self.board[new_coords].append(piece_name)
        else:
            self.board[new_coords] = [piece_name]

        # expand history
        self.history.append({
            "move_type": "move",
            "piece": piece_name,
            "coords": new_coords,
            "previous_coords": old_coords
        })
        # update turn and player
        self.turn += 1
        self.current_player = 'b' if self.current_player == 'w' else 'w'
        return



    def _is_board_connected(self, exclude_coords : tuple[int, int] | list[int] | None = None)->bool:
        """
        Internal method to check if the pieces on the board form a single connected group, excluding any pieces at the specified coordinates.
        This is equivalent to check for One Hive Rule.

        Parameters
        ----------
        exclude_coords : tuple[int, int] | list[int] | None, optional
            The coordinates of a piece to exclude from the connectivity check, by default None

        Returns
        -------
        bool
            True if the board is connected, False otherwise.
        """
        if not self.board or (exclude_coords and exclude_coords in self.board and self._get_stack_height(exclude_coords) == 1):
            return True
        
        all_coords = list(self.board.keys())
        if exclude_coords and exclude_coords in all_coords:
            all_coords.remove(exclude_coords)

        if not all_coords:
            return True
        
        start_coord = all_coords[0]
        queue = collections.deque([start_coord])
        visited = {start_coord}

        while queue:
            current = queue.popleft()
            for neigh in self._get_neighbours(piece_coords=current):
                if neigh in self.board and neigh != exclude_coords and neigh not in visited:
                    visited.add(neigh)
                    queue.append(neigh)

        return len(visited) == len(all_coords) 
    


    def _get_neighbours(self, piece_name : str | None = None, piece_coords : tuple[int, int] | None = None)->list[tuple[int, int]]:
        """ 
        Internal method to get the neighboring coordinates of a piece on the board. 
        Accepts both the piece name or its coordinates as input; if both are provided, method returns are base on the piece name.

        Parameters
        ----------
        piece_name: str | None
            The name of the piece (e.g., 'wQ', 'bS1'). If provided, the method will look up its coordinates on the board.
        piece_coords: tuple[int, int] | None
            The axial coordinates (q, r) of the piece. If provided, the method will use these coordinates directly.

        Returns
        -------
        list of tuple[int, int]
            A list of neighboring coordinates around the piece. Each neighbor is represented as a tuple (q, r).
        """
        assert (piece_name is not None) or (piece_coords is not None), "Either piece_name or piece_coords must be provided."
        assert piece_name in PIECES if piece_name is not None else True, "Invalid piece name provided."
        
        if piece_name is not None:
            piece_q, piece_r = self.pieces[piece_name]
            return [(piece_q + dq, piece_r + dr) for dq, dr in DIRECTIONS]
        else:
            piece_q, piece_r = piece_coords
            return [(piece_q + dq, piece_r + dr) for dq, dr in DIRECTIONS]
    
    
    
    def _get_top_piece(self, piece_coords : tuple[int, int])->str:
        """
        Internal method to get the top piece at a given coordinate, if multiple pieces are stacked.

        Parameters
        ----------
        piece_coords : tuple[int, int]
            The axial coordinates (q, r) of the stack.

        Returns
        -------
        str | None
            The name of the top piece at the given coordinates, or None if there are no pieces.
        """
        assert piece_coords in self.board, f'Piece coordinates must be in the board'

        return self.board[piece_coords][-1]  # Return the last piece in the list (top of the stack)    



    def _get_stack_height(self, piece_coords : tuple[int, int])->int:
        """
        Internal method to get the height of the stack at a given coordinate.

        Parameters
        ----------
        piece_coords : tuple[int, int]
            The axial coordinates (q, r) of the stack.

        Returns
        -------
        int
            The number of pieces stacked at the given coordinates, or 0 if there are no pieces.
        """
        if piece_coords in self.board:
            return len(self.board[piece_coords])  # Return the number of pieces in the stack
        return 0
    


    def _is_place_occupied(self, piece_coords : tuple[int, int])->bool:
        """
        Internal method to check if a given coordinate is occupied by any piece.

        Parameters
        ----------
        piece_coords : tuple[int, int]
            The axial coordinates (q, r) to check for occupation.

        Returns
        -------
        bool
            True if the coordinate is occupied by at least one piece, False otherwise.
        """
        return piece_coords in self.board and len(self.board[piece_coords]) > 0
    


    def _count_neighbors(self, piece_coords : tuple[int, int], exclude_self : tuple[int, int] | None = None)->int:
        """
        Internal method for counting neighbours of a given piece

        Parameters
        ----------
        piece_coords : tuple[int, int]
            The axial coordinates (q, r) of the piece
        exclude_self : tuple[int, int] | None, optional
            The axial coordinates (q, r) of a piece to exclude from the count, by default None

        Returns
        -------
        int
            Count of neighbours
        """
        count = 0
        for neigh_coord in self._get_neighbours(piece_coords=piece_coords):
            if self._is_place_occupied(neigh_coord) and neigh_coord != piece_coords:
                count += 1
        return count
