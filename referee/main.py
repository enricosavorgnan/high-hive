import datetime
from enum import Enum
import time
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


def start_container(name, image_name="mzinga", gpu_id=None):
    gpu_string = f"--gpus {gpu_id}" if gpu_id is not None else ""
    log_dir = os.path.join(os.getcwd(), "logs").replace("\\", "/") # Docker on Windows doesn't like backslashes in volume paths
    os.makedirs(log_dir, exist_ok=True)

    cmd = f"docker run --name {name} -i --rm {gpu_string} -v {log_dir}:/logs -e ENGINE_NAME={name} {image_name}"
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


def play_game(white_image, black_image, white_gpu=None, black_gpu=None):
    referee = start_container("referee", image_name="nokamute")
    white = start_container("white", white_image, gpu_id=white_gpu)
    black = start_container("black", black_image, gpu_id=black_gpu)

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


def parse_version(image):
    """Extract the tag from a Docker image string (`name[:tag]`).

    Returns 'latest' when no tag is provided. Used to track which build of a
    bot played a given game, so benchmarks before/after a code change can be
    compared in the same DB.
    """
    if ":" in image:
        return image.split(":", 1)[1]
    return "latest"


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
        white_version  text,
        black_version  text,
        outcome        text,
        outcome_reason text,
        game_string    text,
        elapsed_s      real
    );
    """)
    # Schema migration: add version columns to pre-existing DBs that were
    # created before the columns were introduced.
    existing_cols = {row[1] for row in db.execute("PRAGMA table_info(games)").fetchall()}
    for col in ("white_version", "black_version"):
        if col not in existing_cols:
            db.execute(f"ALTER TABLE games ADD COLUMN {col} text")
    db.commit()
    return db


def play_tournament(match_list, white_gpu, black_gpu, resume=False):
    for white, black in match_list:
        white_version = parse_version(white)
        black_version = parse_version(black)

        if resume:
            with get_db() as db:
                res = db.execute(
                    """SELECT timestamp FROM games
                       WHERE white = ? AND black = ?
                         AND white_version IS ? AND black_version IS ?""",
                    [white, black, white_version, black_version],
                )
                if res.fetchone() is not None:
                    logging.info(
                        "match %s (%s) vs %s (%s) already played, skipping (--resume)",
                        white, white_version, black, black_version,
                    )
                    continue

        date = datetime.datetime.now().isoformat()
        logging.info(
            "playing game between %s (white) (gpu %s) and %s (black) (gpu %s)",
            white,
            white_gpu,
            black,
            black_gpu,
        )
        result = play_game(white, black, white_gpu=white_gpu, black_gpu=black_gpu)
        with get_db() as db:
            db.execute(
                """INSERT INTO games
                   (timestamp, white, black, white_version, black_version,
                    outcome, outcome_reason, game_string, elapsed_s)
                   VALUES
                   (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                [
                    date,
                    white,
                    black,
                    white_version,
                    black_version,
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
    parser.add_argument(
        "--mirror",
        action="store_true",
        help="For each (white, black) pairing also play (black, white). "
             "Removes color bias from the leaderboard.",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        help="Skip pairings that already have a row in the DB for the same "
             "(white, black, white_version, black_version) tuple.",
    )

    args = parser.parse_args()

    # if args.update_images:
    #     print("Updating Docker images...")
    #     update_images(args.update_images)

    for i in range(int(args.runs) if args.runs else 1):
        print(f"\n\n\n------------------")
        print(f"Running game {i+1}/{int(args.runs) if args.runs else 1}")
        print(f"------------------")
        match_list = load_tournament(args.games)
        if args.mirror:
            match_list = match_list + [(b, w) for (w, b) in match_list]
        play_tournament(match_list, args.white_gpu, args.black_gpu, resume=args.resume)
