import random
import os
import sys

from board import HiveBoard

class MoveEngine:
    """
    Class for engine tools for deciding valid and intriguing moves for a Hive game.
    """

    def __init__(self):
        pass


    def _get_valid_placings(self, board : HiveBoard)->list[str]:
        """
        Returns a list of valid moves for PLACING a piece. 

        Parameters
        ----------
        board : dict
            Hive board configuration
        pieces : dict
            Hive pieces configuration

        Returns
        -------
        list[str]
            Valid placings
        """
        moves = []
        hand = board.white_hand if board.current_player is "w" else board.black_hand
        placed_pieces = [piece for piece in board.pieces if piece.startswith(board.current_player)]
        played_queen = any(piece[1] == 'Q' for piece in placed_pieces)

        # First move
        if not board.board:
            return hand
        
        # Queen Rule
        if board.turn == 4 and not played_queen:
            valid_placings = [piece for piece in hand if piece[1] == 'Q']
        elif (played_queen and len(hand) > 0 ) or not played_queen:
            valid_placings = hand
        else:
            valid_placings = []
        
        # Check for valid spots
        valid_spots = set()
        for piece_coords, piece_name in board.board.items():
            # Look only around current player pieces
            if piece_name.startswith(board.current_player):

                for neigh in board._get_neighbours(piece_coords=piece_coords):
                    # Occupied place
                    if neigh in board.board:
                        continue

                    # Enemies rule
                    enemy_neighs = False
                    if board.turn > 0:
                        for neigh_neigh in board._get_neighbours(piece_coords=neigh):
                            if neigh_neigh in board.board:
                                neigh_name = board.board[neigh_neigh]
                                if neigh_name.startswith(board.current_player):
                                    enemy_neighs = True
                                    break
                    if not enemy_neighs:
                        valid_spots.add(neigh)

        






        return moves

    def _get_valid_movements(self, board : HiveBoard)->list[str]:
        return []

    def get_valid_moves(self, board : HiveBoard)->list[str]:
        """
        Returns all the valid moves given a board configuration.

        Parameters
        ----------
        board : dict
            Hive board defined by HiveBoard class
        pieces : dict
            Hive pieces played

        Returns
        -------
        list[str]
            List with all the possible moves
        """
        valid_placings = self._get_valid_placings(board=board)
        valid_movements = self._get_valid_movements(board=board)
        valid_placings.extend(valid_movements)
        
        return valid_placings 


    def get_best_move(self, board : dict, pieces : dict)-> str | None:
        pass


    def undo_last_move(self, board : HiveBoard)->bool:
        """
        Method for undoing last move

        Returns:
        bool
            True if operation goes right, False if history is empty
        """
        if not board.history: 
            return False
        
        last_move = board.history.pop()
        move_type, piece_name, piece_coords, piece_previous_coords = last_move

        # If move was a placement, remove form board and add to hand
        if move_type == 'place':
            # Remove from pieces
            del board.pieces[piece_name]
            # Remove from board
            board.board[piece_coords].remove(piece_name)
            if not board.board[piece_coords]:
                del board.board[piece_coords]
            # Add to hand
            color = 'w' if piece_name.startswith('w') else 'b'
            if color == 'w':
                board.white_hand.append(piece_name)
            else:
                board.black_hand.append(piece_name)

        # Move was a movement
        else:
            # Move piece back to previous coords
            board.board[piece_previous_coords].append(piece_name)
            board.board[piece_coords].remove(piece_name)
            if not board.board[piece_coords]:
                del board.board[piece_coords]


        return True




class RandomMoveEngine(MoveEngine):
    def __init__(self):
        super().__init__()

    def get_best_move(self, board : HiveBoard)->str | None:
        valid_moves = self.get_valid_moves(board)

        move = random.choice(valid_moves) if valid_moves else None
        return move    