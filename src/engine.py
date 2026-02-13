import random
import os
import sys
import collections

from board import HiveBoard

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


    def _queen_pillbug_move(self, board : HiveBoard, piece_coords : tuple[int, int], valid_targets : set)->None:
        for neigh_coords in board._get_neighbours(piece_coords=piece_coords):
            if not board._is_place_occupied(neigh_coords):
                if self._can_slide(board, piece_coords, neigh_coords):
                    if board._count_neighbors(neigh_coords, exclude_self=piece_coords) > 0:
                        valid_targets.add(neigh_coords)
        return

    def _beetle_move(self, board : HiveBoard, piece_coords : tuple[int, int], valid_targets : set)->None:
        for neigh_coords in board._get_neighbours(piece_coords=piece_coords):
            if board._is_place_occupied(neigh_coords):
                valid_targets.add(neigh_coords)
            else:
                if self._can_slide(board=board, piece_coords=piece_coords, target_coords=neigh_coords) and board._count_neighbors(neigh_coords, exclude_self=piece_coords) > 0:
                    valid_targets.add(neigh_coords)
        return

    def _spider_move(self, board : HiveBoard, piece_coords : tuple[int, int], valid_targets : set)->None:
        """
        Spider moves around the perimeter of the board. This is very expensive.
        By now, only a DFS of depth 5 is appied, no backtracking, no sliding rules 
        """
        queue = collections.deque([(piece_coords, 0, [])])
        while queue:
            (q, r), depth, path = queue.popleft()
            if depth == 5:
                valid_targets.add((q, r))
                continue

            for neigh_coords in board._get_neighbours(piece_coords=(q, r)):
                if not board._is_place_occupied(neigh_coords) and neigh_coords not in path and neigh_coords != piece_coords:
                    if self._can_slide(board, (q, r), neigh_coords) and board._count_neighbors(neigh_coords, exclude_self=piece_coords) > 0:
                        new_path = path + [(q, r)]
                        queue.append((neigh_coords, depth +1, new_path))

    def _ant_move(self, board : HiveBoard, piece_coords : tuple[int, int], valid_targets : set)->None:
        visited = {piece_coords}
        queue = collections.deque([piece_coords])
        while queue:
            curr = queue.popleft()
            for neigh_coords in board._get_neighbours(piece_coords=curr):
                if not board._is_place_occupied(neigh_coords) and neigh_coords not in visited:
                    if self._can_slide(board=board, piece_coords=curr, target_coords=neigh_coords) and board._count_neighbors(neigh_coords, exclude_self=piece_coords) > 0:
                        visited.add(neigh_coords)
                        valid_targets.add(neigh_coords)
                        queue.append(neigh_coords)
        return

    def _ladybug_move(self, board : HiveBoard, piece_coords : tuple[int, int], valid_targets : set)->None:
        # Step 1: Up
        up = []
        for neigh_coords in board._get_neighbours(piece_coords=piece_coords):
            if board._is_place_occupied(neigh_coords):
                up.append(neigh_coords)
        # Step 2: Keep
        keep = []
        for step_up_coords in up:
            for neigh_coords in board._get_neighbours(piece_coords=step_up_coords):
                if board._is_place_occupied(neigh_coords):
                    keep.append(neigh_coords)
        # Step 3: Down
        down = []
        for step_keep_coords in keep:
            for neigh_coords in board._get_neighbours(piece_coords=step_keep_coords):
                if not board._is_place_occupied and neigh_coords != piece_coords:
                    valid_targets.add(neigh_coords)
        return

    def _grassopher_move(self, board : HiveBoard, piece_coords : tuple[int, int], valid_targets : set)->None:
        for dq, dr in DIRECTIONS:
            q, r = piece_coords[0]+dq, piece_coords[1]+r
            if board._is_place_occupied(piece_coords=(q, r)):
                while board._is_place_occupied((q, r)):
                    q += dq
                    r += dr
                valid_targets.add((q, r))
        return


    def _get_valid_placements(self, board : HiveBoard)->list[str]:
        """
        Returns a list of valid moves for PLACING a piece. 

        Parameters
        ----------
        board : HiveBoard
            Hive board configuration 

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
        """
        Returns all the possible pieces moves on a given board

        Parameters
        ----------
        board : HiveBoard
            Hive board configuration

        Returns
        -------
        list[str]
            List of possible movements
        """
        moves = []
        hand = board.white_hand if board.current_player is "w" else board.black_hand
        opposite_player = "b" if board.current_player is "w" else "w"
        played_pieces = [piece for piece in board.pieces if piece.startswith(board.current_player)]

        # No Queen, No Move Rule
        played_queen = any(piece[1] == "Q" for piece in played_pieces)
        if not played_queen: 
            return []
        
        # Check for all pieces the possible movements
        for piece_name in played_pieces:
            piece_coords = board.pieces[piece_name]
            piece_type = piece_name[1]

            # Piece must be on top
            if board._get_top_piece(piece_coords=piece_coords) != piece_name:
                continue

            # One Hive Rule
            if not board._is_board_connected(exclude_coords=piece_coords):
                continue

            # Mosquito Logic
            if piece_type == "M":
                # On stack, acts as B
                if board._get_stack_height(piece_coords) > 1:
                    move_types = ['B']
                else:
                    move_types = set()
                    for neigh in board._get_neighbours(piece_coords=piece_coords):
                        if board._is_place_occupied(neigh):
                            neigh_type = board._get_top_piece(neigh)[1]
                            if neigh != 'M':
                                move_types.add(neigh_type)
                    if not move_types:
                        continue
                    move_types = list(move_types)
            else:
                move_types = [piece_type]

            valid_targets = set()

            for kind in move_types:

                # Queen and Pillbug
                if kind in ['Q', 'P']:
                    self._queen_pillbug_move(board=board, piece_coords=piece_coords, valid_targets=valid_targets)

                # Beetle
                elif kind == 'B':
                    self._beetle_move(board, piece_coords, valid_targets)

                # Spider
                elif kind == 'S':
                    self._spider_move(board, piece_coords, valid_targets)

                # Ant
                elif kind == 'A':
                    self._ant_move(board, piece_coords, valid_targets)

                # Ladybug
                elif kind == 'L':
                    self._ladybug_move(board, piece_coords, valid_targets)

                # Grassopher
                elif kind == 'G':
                    self._grassopher_move(board, piece_coords, valid_targets)


        
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