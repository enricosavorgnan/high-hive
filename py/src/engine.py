import datetime
import sys
import os
import random
import time
from board import HiveBoard
from ai import RandomMoveEngine

def uhp_handler():
    move_engine = RandomMoveEngine()
    board = HiveBoard()


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
                print("Mosquito;Ladybug;Pillbug;")
                print("ok")      
                sys.stdout.flush()


            elif cmd == "newgame":
                # newgame
                # newgame <Base+MLP;State;Player[Turn]>
                # newgame <Base+MLP;State;Player[Turn];GameString>

                # merge all the chunks except the first
                game_string = " ".join(chunks[1:]) if len(chunks) > 1 else ""
                game_string = board.parse_gamestring(gamestring=game_string)
                print(game_string)
                print("ok")
                sys.stdout.flush()


            elif cmd == "play":
                # play <MoveString>

                # assume move is valid
                move_string = " ".join(chunks[1:]) if len(chunks) > 1 else ""
                # update the board
                board.apply_move(move_string=move_string)
                print(move_string)
                print("ok")
                sys.stdout.flush()


            elif cmd == "pass":
                # TODO
                # pass is (should be ?) equal to a play move with empty string
                pass_move = 'pass_string'
                board.parse_gamestring(gamestring=pass_move)
                print(pass_move)
                print("ok")
                sys.stdout.flush()


            elif cmd == "validmoves":
                valid_moves = move_engine.get_valid_moves(board)
                if valid_moves:
                    print(valid_moves)
                else:
                    print("pass")
                sys.stdout.flush()


            elif cmd == "bestmove":
                # bestmove time <TimeInSeconds>
                assert len(chunks) == 3, f"Expected 3 chunks for 'bestmove' command, got {len(chunks)}"
                assert chunks[1] == "time", "Expected 'time' after 'bestmove', currently 'depth' is not supported"

                best_move = move_engine.get_best_move(board)
                print(best_move)
                sys.stdout.flush()


            elif cmd == "undo":
                # TODO: check for how Mzinga deals with this
                if move_engine.undo_last_move(board=board):
                    print("ok")
                sys.stdout.flush()

            elif cmd == "exit":
                break


        except Exception as e:
            # Exception Information
            exc_type, exc_obj, exc_tb = sys.exc_info()
            # Extract line
            line_number = exc_tb.tb_lineno
            # Extract filename
            fname = os.path.split(exc_tb.tb_frame.f_code.co_filename)[1]
            
            with open('log/uhp.log', 'a') as f:
                f.write(f'{datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}\t\t[File: {fname} | Line: {line_number}]\t\tError: {e} \t\n')
        
        finally:
            pass


if __name__ == "__main__":
    uhp_handler()