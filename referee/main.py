import datetime
from enum import Enum
import time
import datetime
import subprocess as sp
import shlex
import logging
import textwrap
from dataclasses import dataclass
import os
import sqlite3
import argparse
import yaml

logging.basicConfig(level=logging.INFO)


GAME_TYPE = "Base+MLP"
MOVE_TIMEOUT_S = 5
MAX_PLIES = 100


def read_message(process, timeout=None):
    MAX_LINES = 100
    os.set_blocking(process.stdout.fileno(), False)
    msg = ""
    line = ""
    line_cnt = 0
    is_error = False
    start = time.time()
    while line != "ok":
        if timeout is not None and time.time() - start > timeout:
            raise TimeoutError("Timeout exceeded")
        line = process.stdout.readline().strip()
        if line:
            if line.startswith("err"):
                is_error = True
            msg += line + "\n"
            line_cnt += 1
            if line_cnt > MAX_LINES:
                raise IOError("Too many lines")
        time.sleep(0.1)
    msg = msg.strip("\nok")
    if is_error:
        raise ValueError(f"protocol error: {msg}")
    logging.debug("message from %d:\n%s", process.pid, textwrap.indent(msg, "    "))
    return msg


def send_message(msg, process):
    logging.debug("message to %d:\n%s", process.pid, textwrap.indent(msg, "    "))
    process.stdin.write(msg + "\n")
    process.stdin.flush()



def create_stats_folder(runs):
    now = datetime.datetime.now()
    datetime_str = now.strftime("%Y-%m-%d_%H-%M-%S")
    log_dir = os.path.join(os.getcwd(), "logs", datetime_str)
    os.makedirs(log_dir, exist_ok=True)

    for run in range(runs):
        open(os.path.join(log_dir, f"black/{run}.log"), "w").close()
        open(os.path.join(log_dir, f"white/{run}.log"), "w").close()

    return datetime_str


def start_container(name, image_name="mzinga", gpu_id=None, stats=False, now_time=None, run=None, color=None):
    print(f"Starting container {name} with image {image_name} on GPU {gpu_id}")
    gpu_string = f"--gpus {gpu_id}" if gpu_id is not None else ""
    log_dir = os.path.join(os.getcwd(), "logs").replace("\\", "/") # Docker on Windows doesn't like backslashes in volume paths
    os.makedirs(log_dir, exist_ok=True)

    cmd = f"docker run --name {name} -i --rm {gpu_string} -v {log_dir}:/logs -e ENGINE_NAME={name} {image_name}"

    if stats:
        assert datetime is not None, f"datetime {datetime} must not be None"
        assert run is not None, f"run {run} must not be None"
        log_path = f"./logs/{now_time}/{color}/{run}.log"
        cmd += f"--verbose --log-path {log_path}"
    logging.debug("running: `%s`", cmd)
    child = sp.Popen(
        shlex.split(cmd),
        stdin=sp.PIPE,
        stdout=sp.PIPE,
        stderr=sp.PIPE,
        text=True,
    )
    # wait for the container to start, and if it fails, abort
    time.sleep(5)
    assert child.poll() is None, f"stdout:\n{child.stdout.read()}\nstderr:\n{child.stderr.read()}"
    logging.info("start %s, PID %d", name, child.pid)
    return child


class Outcome(Enum):
    WHITE_WINS = "WhiteWins"
    BLACK_WINS = "BlackWins"
    DRAW = "Draw"


@dataclass
class GameOutcome(object):
    outcome: Outcome
    reason: str
    game_string: str
    elapsed_s: float


def do_play_game(referee, white, black):
    # Start the game for each engine
    for sub in [referee, white, black]:
        send_message(f"newgame {GAME_TYPE}", sub)
        msg = read_message(sub, timeout=1)

    # The string describing the status of the board
    game_string = ""
    start_time = time.time()

    for ply in range(MAX_PLIES):
        is_white = ply % 2 == 0
        logging.info("ply: %d is_white: %d", ply, is_white)
        if is_white:
            current = white
        else:
            current = black

        send_message(f"bestmove time 00:00:{MOVE_TIMEOUT_S:02}", current)
        # Enforce the timeout, while allowing an additional grace period
        try:
            move = read_message(current, timeout=MOVE_TIMEOUT_S + 1)
        except:
            return GameOutcome(
                Outcome.BLACK_WINS if is_white else Outcome.WHITE_WINS,
                reason="timeout while recommending best move",
                game_string=game_string,
                elapsed_s=time.time() - start_time,
            )
        # check that the move is valid by first applying it to the referee
        send_message(f"play {move}", referee)
        ans = read_message(referee)
        if ans.startswith("invalidmove"):
            if is_white:
                return GameOutcome(
                    Outcome.BLACK_WINS,
                    reason=f"white proposed invalid move: `{move}`",
                    game_string=game_string,
                    elapsed_s=time.time() - start_time,
                )
            else:
                return GameOutcome(
                    Outcome.WHITE_WINS,
                    reason=f"black proposed invalid move: `{move}`",
                    game_string=game_string,
                    elapsed_s=time.time() - start_time,
                )
        else:
            game_string = ans
            logging.info("game string:\n%s", textwrap.indent(game_string, "    "))

        game_state = game_string.split(";")[1]
        if game_state in ["Draw", "WhiteWins", "BlackWins"]:
            outcome = Outcome(game_state)
            logging.info("The game ended: %s", outcome)
            return GameOutcome(
                outcome,
                reason="normal ending",
                game_string=game_string,
                elapsed_s=time.time() - start_time,
            )

        # Apply the moves
        for i, player in enumerate([white, black]):
            send_message(f"play {move}", player)
            try:
                ans = read_message(player)
            except ValueError as e:
                # Se un giocatore genera un errore di protocollo nel processare una mossa valida...
                # ... perde la partita.
                if i == 0: # Era il bot bianco
                    return GameOutcome(
                        Outcome.BLACK_WINS,
                        reason=f"white failed to process valid move `{move}` with error: {e}",
                        game_string=game_string,
                        elapsed_s=time.time() - start_time,
                    )
                else: # Era il bot nero
                    return GameOutcome(
                        Outcome.WHITE_WINS,
                        reason=f"black failed to process valid move `{move}` with error: {e}",
                        game_string=game_string,
                        elapsed_s=time.time() - start_time,
                    )
            if ans.startswith("invalidmove"):
                if i == 0:
                    return GameOutcome(
                        Outcome.BLACK_WINS,
                        reason="unrecognized valid move by white",
                        game_string=game_string,
                        elapsed_s=time.time() - start_time,
                    )
                else:
                    return GameOutcome(
                        Outcome.WHITE_WINS,
                        reason="unrecognized valid move by black",
                        game_string=game_string,
                        elapsed_s=time.time() - start_time,
                    )

    return GameOutcome(
        Outcome.DRAW,
        reason="maxed out plies",
        game_string=game_string,
        elapsed_s=time.time() - start_time,
    )


def play_game(white_image, black_image, white_gpu=None, black_gpu=None, stats=False, now_time=None, run=None):
    referee = start_container("referee", image_name="nokamute", stats=False)
    white = start_container("white", white_image, gpu_id=white_gpu, stats=stats, now_time=now_time, run=run, color="white")
    black = start_container("black", black_image, gpu_id=black_gpu, stats=stats, now_time=now_time, run=run, color="black")

    # Get the greetings
    for sub in [referee, white, black]:
        logging.debug("Getting the greeting")
        msg = read_message(sub, timeout=10)
        logging.debug("Got %s", msg)
        # Check the they export all the extensions
        # FIXME: handle errors gracefully
        print(f"Player greeting: {msg}")
        assert msg.split("\n")[1].strip() == "Mosquito;Ladybug;Pillbug"

    outcome = do_play_game(referee, white, black)

    for container in ["white", "black", "referee"]:
        logging.info(f"killing container {container}")
        sp.run(["docker", "kill", container])
    # wait for Docker to clean up containers
    time.sleep(5)

    return outcome


def get_db():
    db_path = os.path.join(os.getcwd(), "databases", "games_aze.db")
    #if exists a path
    if not os.path.exists(db_path):
        db_path = os.path.join(os.getcwd(), "./referee/databases", "games_aze.db")

    db = sqlite3.connect(db_path)
    db.executescript("""
    CREATE TABLE IF NOT EXISTS games (
        timestamp      text,
        white          text,
        black          text,
        outcome        text,
        outcome_reason text,
        game_string    text,
        elapsed_s      real
    );
    """)
    return db


def play_tournament(match_list, white_gpu, black_gpu, stats=False, now_time=None, run=None):
    for white, black in match_list:
        # with get_db() as db:
        #     res = db.execute(
        #         "SELECT timestamp FROM games WHERE white = ? AND black = ?",
        #         [white, black]
        #     )
        #
        #     if res.fetchone() is not None:
        #         logging.info(
        #             "match between %s and %s already played",
        #             white, black
        #         )
        #         continue

        date = datetime.datetime.now().isoformat()
        logging.info(
            "playing game between %s (white) (gpu %s) and %s (black) (gpu %s)",
            white,
            white_gpu,
            black,
            black_gpu,
        )
        result = play_game(white, black, white_gpu=white_gpu, black_gpu=black_gpu, stats=stats, now_time=now_time, run=run)
        with get_db() as db:
            db.execute(
                """INSERT INTO games
                   (timestamp, white, black, outcome, outcome_reason, game_string, elapsed_s)
                   VALUES
                   (?, ?, ?, ?, ?, ?, ?)
                """,
                [
                    date,
                    white,
                    black,
                    result.outcome.value,
                    result.reason,
                    result.game_string,
                    result.elapsed_s,
                ],
            )


def load_tournament(path):
    match_list = []
    with open(path) as fp:
        for line in fp.readlines():
            if line.startswith("#") or len(line.strip()) == 0:
                continue
            tokens = tuple(t.strip() for t in line.split(","))
            assert len(tokens) == 2
            match_list.append(tokens)

    return match_list


def update_images(images_path : str):
    """
    Update Docker images for white and black players.
    """
    with open(images_path, 'r') as f:
        config = yaml.safe_load(f)

    for player in ['white', 'black']:
        update_player = f"update_" + player
        docker_player = f"docker_" + player
        if config.get(update_player):
            docker_command = config.get(docker_player)
            logging.info(f"Updating {player} image with command: {docker_command}")
            result = sp.run(docker_command, shell=True, capture_output=True, text=True)
            if result.returncode == 0:
                logging.info(f"Successfully updated {player} image: {result.stdout}")
            else:
                logging.error(f"Failed to update {player} image: {result.stderr}")

    return



if __name__ == "__main__":
    parser = argparse.ArgumentParser("referhive")
    parser.add_argument("--games", default="tournament.txt")
    parser.add_argument("--white-gpu")
    parser.add_argument("--black-gpu")
    parser.add_argument("--runs")
    parser.add_argument("--update-images", default="images.yaml")
    parser.add_argument("--stats", default=False)

    args = parser.parse_args()

    runs = int(args.runs) if args.runs else 1
    stats = args.stats
    if stats:
        now_time = create_stats_folder(runs)

    for i in range(runs):
        print(f"\n\n\n------------------")
        print(f"Running game {i+1}/{int(args.runs) if args.runs else 1}")
        print(f"------------------")
        match_list = load_tournament(args.games)
        play_tournament(match_list, args.white_gpu, args.black_gpu, stats=stats, now_time=now_time, run=i)
