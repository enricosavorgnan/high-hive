import sys
import os
import random
import time
from board import HiveBoard
from engine import RandomMoveEngine

def uhp_handler():
    board = HiveBoard(move_engine=RandomMoveEngine)

    while True:
        try:
            # each new command is in a new standard input line
            line = sys.stdin.readline()
            if not line: 
                break     
            line = line.strip()
            if not line:
                continue

            # split at each space " "
            chunks = line.split()    
            # first chunk is the command        
            cmd = chunks[0]         

            # Accepted commands are:
            # "u1"
            # "info"
            # "newgame"
            # "validmoves"
            # "bestmove" --> important! sets also the time
            # "play"
            # "pass"
            # "undo"
            # "options"
            # "exit"
            assert cmd in ['u1', 'info', 'newgame', 'play', 'validmoves', 'bestmove', 'pass', 'undo', 'options', 'exit'], f"Command {cmd} not found"

            if cmd == "u1":
                print('ok')
                sys.stdout.flush()


            elif cmd == "info":
                print("id high-hive-engine v0.1")
                # print capabilities
                print("mosi") 
                print("ok")      
                sys.stdout.flush()


            elif cmd == "newgame":
                # newgame <BaseBoard;SomeThing>

                # merge all the chunks except the first
                game_string = " ".join(chunks[1:])
                board.parse_gamestring(game_string)
                print(game_string)
                sys.stdout.flush()


            elif cmd == "play":
                # play <MoveString>

                # assume move is valid
                move = chunks[1]
                # update the board
                board.parse_gamestring(gamestring=move)
                # just an echo
                print(move)
                sys.stdout.flush()


            elif cmd == "pass":
                # TODO
                # pass is (should be ?) equal to a play move with empty string
                pass_move = 'pass_string'
                board.parse_gamestring(gamestring=pass_move)
                print(pass_move)
                sys.stdout.flush()


            elif cmd == "validmoves":
                valid_moves = RandomMoveEngine().get_valid_moves(board.board)
                if valid_moves:
                    print(" ".join(valid_moves))
                else:
                    print("pass")
                sys.stdout.flush()


            elif cmd == "bestmove":
                # bestmove time <TimeInSeconds>

                best_move = RandomMoveEngine().get_best_move(board)
                print(best_move)
                sys.stdout.flush()


            elif cmd == "undo":
                # TODO: check for how Mzinga deals with this
                if board.undo_last_move():
                    print("ok")
                sys.stdout.flush()

            elif cmd == "exit":
                break


        except Exception as e:
            with open('uhp_error.log', 'a') as f:
                f.write(f'Error: {e}\n') 
        
        finally:
            pass


if __name__ == "__main__":
    uhp_handler()