# High Hive C++ Implementation

- Version **0.1**
- Last modify: **13/03/2026**
- Last modifier: **Enrico**


*The following is a description of the Hive Bot implementation in *C++* language*. \
*Part of the notes assumes a Referee that manages an Hive play game like the one implemented in this [GitHub repository](https://github.com/Cecca/referhive/tree/main)*. \
*If you change the code in `cpp/src/` folder, please make sure to update also this document!*.

## 1. Structure of `src/` folder
``` markdown
└── src/                        # Source code for C++ implementation   
    ├── headers/                # Headers file
        ├── board.h             # describes the board 
        ├── coords.h            # describes the coordinate system 
        ├── engine.h            # defines the engine for choosing the best move
        ├── generator.h         # generates all the possible valid moves
        ├── moves.h             # defines the pieces moves
        ├── pieces.h            # defines the pieces
        ├── rules.h             # takes care of all the Hive rules
        ├── state.h             # defines the state of the play game
        ├── uhp.h               # defines the protocol and bridges the inputs to the backend
        └── utils.h             # defines utils functions for the UHP implementation
        
    ├── board.cpp
    ├── engine.cpp
    ├── generator.cpp
    ├── moves.cpp
    ├── rules.cpp
    ├── state.cpp
    ├── uhp.cpp
    ├── utils.cpp
    └── MAIN.CPP                # entry point to the UHP bot 
```

---

## 2. The Bot flux
The entry point to play against High Hive bot is `main.cpp`. \
The first action is the so-called *greeting* between the two players, meaning that each of them shows its name, its version, and the Hive game version which is able to play. \
Currently the **High Hive bot** is at **Version 0.1** meaning that only a random engine is able to play under-the-hood; the bots plays a **Hive Version 'Base+MLP'**, meaning that all the existing extensions are included: *Mosquito*, *Ladybug* and *Pillbug*. 

Passing the greeting, a UHP loop (defined in `uhp.cpp`) starts, meaning that a `state`, an `uhpBoard` and a `moveHistory` are intialized. \
The `state`, in particular, internally defines the board, the current player and its turn and the players hands, plus some useful extra information. \
For a better specification on the state structure read the section [3.8](#38-the-state); for a better specification on the UHP protocol implementation and the `uhpBoard` structure read the section [3.9](#39-the-uhp-protocol). 

The UHP loop waits for some istructions from the Hive Referree, including indications for the game starting, rival bot played moves, queries for the valid moves in a given situation and for the best move, undo request and finally for leaving the game. Queries for options are not currently implemented.

Extra useful information:
- Each command is processed by methods in `uhp.cpp` file.
- The state of the game is managed by the `state.cpp` file.
- The translation from backend moves and UHP moves string, and viceversa, is described in `utils.cpp`.
- The board and the implemented coordinates system are defined respectively in `board.cpp` and `coords.h` files.
- The generation of the valid moves is defined into `generator.cpp`
- The choice of the best valid move is defined into `engine.cpp`

---

## 3. The implementation
The following subfolders present the structure of the methods internally defined. \
The topics discussed are presented with an alphabetical order.

### 3.1 The Board
Different implementations of the board are included into the `board.cpp` file and its relative header `board.h`. \
Currently are supported implementation of a board as:
- Matrix, or Array to be strictly specific ([3.1.1](#311-the-board-as-a-matrix))
- Unordered Map ([3.1.2](#312-the-board-as-a-map))

For chosing among the two, you need to comment or uncomment Line `14` of [board.cpp](src/board.cpp#L14). \
If **commented**, the board uses a Matrix implementation. This is currently the **default** one. \
If **uncommented**, the board uses a Unordered Map implementation.

Which one to use is currently just a matter of preferences. \
No performance benchmarks have been currently developed.

In each of the two cases, a different solution for each cell or tile of the Hive Board have been build.

#### 3.1.1 The Board as a Matrix
Chosing the board as a matrix implementation implies the following:
- Need to choose the [*dimension*](src/headers/board.h#L33) of the board. Current default is `BOARD_DIM = 64`.
- Need to choose the [*height*](src/headers/board.h#L36) of the Cell stack. Current default is `MAX_STACK = 6`. 

Each tile of the board is a `CellStack` as presented in a few moments. \
The `BoardAsArray` method stores not only the `_grid` parameter containing the `CellStacks`, but also a `_occupied_coords` parameter taking care of the occupied coordinates into the cell stack. \
`_occupied_coords` is needed to speeding up the computation when the `occupiedCoords` is queried. \
A `NEIGHBORS` parameter is also build: it defines the coordinates for the 6 neighbors of a given coordinate and is exploited by `getOccupiedNeighbors` method.

Since the common matrix is squared, while Hive has hexagonal tiles, actually the computation of the neighbors is delicated, as discussed in the [Coordinates Section](#32-the-coordinates). Moreover, a matrix in each programming language is actually nothing more than an array. This is why an offset parameter `BOARD_OFFSET` is need to be included. 

The methods implemented are presented in the following.

1. Methods for managing the conversion from Hexagonal Coordinate System into the Axial System 
   - `AxToIndex`
   : Given a axial coordinate, i.e. a coordinate in the hexagonal coordinates system, returns the corresponding index in the board array.
   - `isValid`
   : Returns `true` if a given hexagonal coordinate is valid, i.e. its mapping into an Axial Coordinate is valid.
   
2. Methods for quering the content of a tile 
   - `occupiedCoords`
   : Returns the totality of the occupied tiles stored into `_occupied_coords` parameter
   - `getOccupiedNeighbors`
   : Fills a vector of coordinates with the hexagonal coordinates of the occupied tiles that are neighbors of a tile at a given hexagonal coordinate. 
   The check is performed via a `for` loop.
   - `top`
   : Returns the top bug in a tile at a given hexagonal coordinate. Calls `CellStack.top()`.
   - `height`
   : Returns the height of a tile at a given hexagonal coordinate. Calls `CellStack.height()`.
   - `empty`
   : Returns whether a tile at a given hexagonal coordinate has elements inside or not. Calls `CellStack.empty()`.
   
3. Methods for doing operations in the board 
   - `place`
   : Places a given piece in top of a tile at a given hexagonal coordinate. \
   ! No checks are done regarding the validity of the operation.
   - `remove`
   : Removes the piece at the top of a tile at a given hexagonal coordinate. \
   ! Only one piece for call is removed.
   - `move`
   : Moves the piece at the top of a tile at a given hexagonal coordinate `from` to the top of a tile at a given hexagonal coordinate `to`. \
   Under the hoods the method calls `remove` at first and then method `place`. \
   ! No checks are done regarding the validity of the operation.


Notice how it is currently possible for a piece to exit from the grid. It is the case, for example, of limit scenarios in which some bugs move repeatedly among only one direction. \ This, obviously, should not happen. \
A possible solution can consist into implementing a method that automatically re-centers the Hive once a bug is placed too close to the board. \
Currently, this is left as a TODO.


As already introduced, each tile is a `CellStack`, i.e. a vector with two elements:
- An array of pieces of dimension `MAX_STACK`, default equal to `6`.
- A small integer containing the number of elements included in the cell.

Nothing more to add but the methods implemented.
- `push`, `pop` : Are the classic push operation for inserting an item and classic pop operation for removing an item
- `top` : Retrieves the top element of the cell
- `contains` : Returns whether a piece is in the cell stack
- `empty`: Returns whether the cell contains an element
- `height` : Returns the number of elements contained by a cell
- `clear`: Operates an hard reset of the cell
- `begin` and `end`: Provide iterators for looping on the cell stack.

#### 3.1.2 The Board as a Map
A slightly different approach consists in define the board as an unordered map.
The positive aspect of this choice is that there not exists the problem of moving outside the grid; the negative is that cost of finding a piece into the map is a bit slower (despite scaling with $\mathcal{O}(1)$ since the `UnorderedMap` structure is a kind of hash table). \
Actual gaining in performance need to be tested.

The Unordered Map is implemented wth multiple cells, each one a tuple of elements:
- `Coord` for the tile coordinate;
- `UPCellStack` as a stack of pieces;
- `CoordHash` as a coordinate hashing using Spatial Hashing. 
Moreover, in a way similar to the one in `BoardAsArray` class, a `_occupied_coords` parameter is implemented.

The methods implemented are clearly similar to the ones of the `BoardAsArray` class:
- `top` : Returns the piece in top of a tile at a given haxagonal coordinate.
- `height` : Returns the height of a tile at a given hexagonal coordinate.
- `empty` : Returns whether a tile at a given hecagonal coordinate is empty or not.
- `occupiedCoords` : for returning the occupied coordinates in the map, stored in `_occupied_coords`.
- `getOccupiedNeighbors` : Fills a vector with the neighbors of a cell at given hexagonal coordinate.
- `place`, `remove`, `move` : similar to the ones in `BoardAsArray` class.


### 3.2 The Coordinates

### 3.3 The Engines
#### 3.3.1 The Random Engine

### 3.4 The Generator of the moves

### 3.5 The Moves

### 3.6 The Pieces

### 3.7 The Rules

### 3.8 The State

### 3.9 The UHP Protocol
#### 3.9.1 The UHP Loop
#### 3.9.2 The UHP Utils
