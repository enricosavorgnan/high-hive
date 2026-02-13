# Hive engine
import sys
import os
import random
import time

import collections
from engine import *

# clock-wise directions starting from East up to North-East
DIRECTIONS = [        
    (1, 0), (0, 1), (-1, 1), (-1, 0), (0, -1), (1, -1)
]

# UHP String to Direction Map
UHP_DIR_MAP = {
    (1, 0):  ("-", "Right"), # East
    (-1, 0): ("-", "Left"),  # West
    (0, -1): ("\\", "Left"), # North-West
    (0, 1):  ("\\", "Right"),# South-East
    (1, -1): ("/", "Left"),  # North-East
    (-1, 1): ("/", "Right")  # South-West
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

    def __init__(self, move_engine):
        self.engine : MoveEngine = move_engine
        self._reset()



    def _reset(self)->None:
        self.board : dict[tuple[int, int], list[str]] = {}

        self.pieces : dict = {}
        self.white_hand : list = ['wQ', 'wS1', 'wS2', 'wB1', 'wB2', 'wG1', 'wG2', 'wG3', 'wA1', 'wA2', 'wA3', 'wM', 'wL', 'wP']
        self.black_hand : list = ['bQ', 'bS1', 'bS2', 'bB1', 'bB2', 'bG1', 'bG2', 'bG3', 'bA1', 'bA2', 'bA3', 'bM', 'bL', 'bP']
        self.history : list[dict] = []

        self.turn : int = 0
        self.current_player : str = 'w'



    def parse_gamestring(self, gamestring : str)->int:
        """
        Parses the full 'Base;...' game string from Mzinga and updates the board state accordingly.

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
        chunks = gamestring.split(';')
        assert chunks[0] == 'Base', "Expected gamestring to start with 'Base'"

        for chunk in chunks:
            if chunk.startswith("Turn"):
                self.current_player = chunk.split("=")[1]
            elif chunk.startswith("Move"):
                self.turn = int(chunk.split("=")[1])
            elif chunk.startswith("Pieces"):
                # Expected format is: Pieces=colorPiece[coor_q, coor_r],...
                pieces = chunk.split("=")[1]
                if not pieces:
                    continue

                for piece in pieces.split(","):
                    # now format is: colorPiece[coor_q, coor_r]

                    piece_name, piece_coords = piece.split("[")
                    piece_coords = piece_coords.strip("]")

                    # Now it depends on the Mzinga axial coordinates structure.
                    # Here is supposed the one (q, r) described in notes.md
                    # TODO: check Mzinga coords structure.
                    try:
                        q, r = map(int, piece_coords.split(","))
                        if not self.board.get((q, r)):
                            self._place_piece(piece_name, (q, r))
                        else:
                            self._move_piece(piece_name, (q, r))
                    except ValueError as e:
                        with open("engine.log", "a") as f:
                            f.write(f'ValueError encountered during parsing a string: {e}\n')
                        return 1

                    # remove piece from hand
                    color = 'w' if piece_name.startswith('w') else 'b'
                    if color == 'w':
                        if piece_name in self.white_hand:
                            self.white_hand.remove(piece_name)
                    else:
                        if piece_name in self.black_hand:
                            self.black_hand.remove(piece_name)
                    
            
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
        self.board[piece_coord].append(piece_name)
        # expand history
        self.history.append({
            "move_type": "place",
            "piece": piece_name,
            "coords": piece_coord,
            "previous_coords": None
        })
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
    
    
    
    def _get_top_piece(self, piece_coords : tuple[int, int])->str | None:
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
        if piece_coords in self.board and self.board[piece_coords]:
            return self.board[piece_coords][-1]  # Return the last piece in the list (top of the stack)
        return None
    


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