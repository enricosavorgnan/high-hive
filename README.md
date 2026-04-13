# High-Hive
A protocol for play Hive games using Universal Hive Protocol (UHP) *MZinga* protocol.

## Repository Structure
```
├── cpp/                            # C++ UHP implementation   
|   |               
|   ├── src/                        # Source code for C++ implementation
|   |   |
|   |   ├── headers/                # Headers file
|   |   |   ├── board.h             # describes the board 
|   |   |   ├── coords.h            # describes the coordinate system 
|   |   |   ├── engine.h            # defines the engine for choosing the best move
|   |   |   ├── generator.h         # generates all the possible valid moves
|   |   |   ├── moves.h             # defines the pieces moves
|   |   |   ├── pieces.h            # defines the pieces
|   |   |   ├── rules.h             # takes care of all the Hive rules
|   |   |   ├── state.h             # defines the state of the play game
|   |   |   ├── uhp.h               # defines the protocol and bridges the inputs to the backend
|   |   |   └── utils.h             # defines utils functions for the UHP implementation
|   |   |
|   |   ├── board.cpp
|   |   ├── engine.cpp
|   |   ├── generator.cpp
|   |   ├── moves.cpp
|   |   ├── rules.cpp
|   |   ├── state.cpp
|   |   ├── uhp.cpp
|   |   ├── utils.cpp
|   |   └── MAIN.CPP                # entry point to the UHP bot 
|   |
|   ├── material/daniele        # LEGACY
|   |   ├── 1coord.h            # axial coordinates system, coordinate operators and CoordHAsh
|   |   ├── 2hexgrid.h          # hexgrid directions
|   |   ├── 3piece.h            # pieces, color-names and bug-names
|   |   ├── 4board.h            # board = unordered set< coord, vector<pieces>, coordhash>
|   |   ├── 5move.h             # different kind of moves
|   |   ├── 6state.h            # state of the game, color-turn, color-hand and undo
|   |   ├── 7movegen_place.h    # generate all possible placements
|   |   ├── 8onehive_art.h      # onehive rule: return all articulation points in linear time
|   |   ├── 9movegen_move.h     # generate all possible movements/drags, piece by piece
|   |   └── 10uhp_engine.cpp    # uhp compliant random bot
|   |
|   ├── .gitignore
|   └── CMakeLists.txt
|
├── py/                             # Python UHP implementation
|   ├── log/                        # Log files for UHP interactions
|   |   └── uhp.py
|   ├── src/                        # Source code for Python UHP implementation
|   |   ├── ai.py                   # AI logics, currently RandomMoveEngine
|   |   ├── board.py                # Board representation and logic
|   |   └── engine.py               # Handling communications between High-Hive and Mzinga
|   ├── .gitignore
|   ├── pyproject.toml
|   └── run.bat                     # Batch file required for MzingaViewer
|
├── resources/
|   ├── img/
|   ├── notes.md                    # For Hive notes
|   └── reinforcement.md            # For Reinforcement Learning notes
|
└── README.md
```
For implementation details regarding C++ implementation, please refer to`cpp/notes.md`.

## Usage
[//]: # (TODO: Add contribution guidelines.)

## Contributing
[//]: # (TODO: Add contribution guidelines.)

