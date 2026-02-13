import random
import os
import sys

from board import HiveBoard

UHP_DIR_MAP = {
    (1, 0):  ("-", "Right"), # East
    (-1, 0): ("-", "Left"),  # West
    (0, -1): ("\\", "Left"), # North-West
    (0, 1):  ("\\", "Right"),# South-East
    (1, -1): ("/", "Left"),  # North-East
    (-1, 1): ("/", "Right")  # South-West
}

class MoveEngine:
    """
    Class for engine tools for deciding valid and intriguing moves for a Hive game.
    """

    def __init__(self):
        pass


    def _can_slide(self, board : HiveBoard, piece_coords : tuple[int, int], target_coords : tuple[int, int])->bool:
        """
        Returns if the move from a starting point to and ending point is valid.

        Parameters
        ----------
        piece_coords : tuple[int, int]
            Coordinates of the piece to move
        target_coords : tuple[int, int]
            Coordinates of the desider target place in the board

        Returns
        -------
        bool
            True if the move is valid, False otherwise
        """
        piece_neigh = set(board._get_neighbours(piece_coords=piece_coords))
        target_neigh = set(board._get_neighbours(piece_coords=target_coords))
        neighs = list(piece_neigh.intersection(target_neigh))

        block_neighs = 0
        for neigh in neighs:
            block_neighs += 1 * board._is_place_occupied(neigh)

        if block_neighs == 2:
            return False

        return True



    def _get_valid_placements(self, board : HiveBoard)->list[str]:
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
        opposite_player = "b" if board.current_player is "w" else "w"
        placed_pieces = [piece for piece in board.pieces if piece.startswith(board.current_player)]

        # First move
        if not board.board:
            return hand
        
        # No pieces in hand:
        if not hand:
            return []
        
        # Queen Rule
        played_queen = any(piece[1] == 'Q' for piece in placed_pieces)
        if board.turn == 4 and not played_queen:
            valid_placings = [piece for piece in hand if piece[1] == 'Q']
        elif (played_queen and len(hand) > 0 ) or not played_queen:
            valid_placings = hand
        else:
            valid_placings = []
        
        # Check for valid spots
        valid_spots = set()
        for piece_name in placed_pieces:
            piece_coords = board.pieces[piece_name]

            for neigh in board._get_neighbours(piece_coords=piece_coords):
                # Occupied place
                if neigh in board.board:
                    continue

                # Enemies rule
                enemy_neighs = False
                if board.turn > 0:
                    for neigh_neigh in board._get_neighbours(piece_coords=neigh):
                        if neigh_neigh in board.board:
                            top_piece = board._get_top_piece(neigh_neigh)
                            if top_piece and top_piece.startswith(opposite_player):
                                enemy_neighs = True
                                break
                if not enemy_neighs:
                    valid_spots.add(neigh)


        # Convert sets to valid moves format
        for piece in hand:
            for piece_coords in valid_spots:
                # Must find a reference for UHP string
                ref_piece = None
                ref_relative_coordinates = None

                for neigh in board._get_neighbours(piece_coords):
                    if board._is_place_occupied(neigh):
                        ref_piece = board._get_top_piece(neigh)
                        ref_relative_coordinates = neigh
                        break

                dq = piece_coords[0] - ref_relative_coordinates[0]
                dr = piece_coords[1] - ref_relative_coordinates[1]
                
                # Lookup the symbol
                if (dq, dr) in UHP_DIR_MAP:
                    symbol, side = UHP_DIR_MAP[(dq, dr)]
                    
                    if side == "Left":
                        # Example: \wQ (North West of wQ)
                        move_str = f"{piece} {symbol}{ref_piece}"
                    else:
                        # Example: wQ\ (South East of wQ)
                        move_str = f"{piece} {ref_piece}{symbol}"
                        
                    moves.append(move_str)  

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
        valid_placings = self._get_valid_placements(board=board)
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
        move = random.choice(valid_moves) if valid_moves else "pass"
        return move