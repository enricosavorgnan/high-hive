# High-Hive
A protocol for play Hive games using Universal Hive Protocol (UHP) *MZinga* protocol.

## Repository Structure
```
├── cpp/                        # C++ UHP implementation                  
|   ├── src/
|   └── material/daniele
|       ├── 1coord.h            # axial coordinates system, coordinate operators and CoordHAsh
|       ├── 2hexgrid.h          # hexgrid directions
|       ├── 3piece.h            # pieces, color-names and bug-names
|       ├── 4board.h            # board = unordered set< coord, vector<pieces>, coordhash>
|       ├── 5move.h             # different kind of moves
|       ├── 6state.h            # state of the game, color-turn, color-hand and undo
|       ├── 7movegen_place.h    # generate all possible placements
|       └── 8onehive_art.h      # onehive rule: return all articulation points in linear time
|
├── py/                         # Python UHP implementation
|   ├── log/                    # Log files for UHP interactions
|   |   └── uhp.py
|   ├── src/                    # Source code for Python UHP implementation
|   |   ├── ai.py               # AI logics, currently RandomMoveEngine
|   |   ├── board.py            # Board representation and logic
|   |   └── engine.py           # Handling communications between High-Hive and Mzinga
|   ├── .gitignore
|   ├── pyproject.toml
|   └── run.bat                 # Batch file required for MzingaViewer
|
├── resources/
|   ├── img/
|   ├── notes.md                # For Hive notes
|   └── reinforcement.md        # For Reinforcement Learning notes
|
└── README.md
```

## Usage
[//]: # (TODO: Add contribution guidelines.)

## Contributing
[//]: # (TODO: Add contribution guidelines.)

