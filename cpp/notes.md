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
The choosen hexagonal coordinate system is implemented into `coords.h` file. \
It consists of three coordinates `(q, r, s)`, one for each of the axis of an hexagon. Since the sum of the coordinates presented is always 1, the `s` coordinate can be safely removed.

The relative neighbors of a tile `(q, r)` are defined as follows:
```text
(0, -1)      /      \    (+1, -1)
(-1, 0)     | (q, r) |   (+1, 0)
(-1, +1)     \      /    (0, +1)
```

Regarding the implementation, each type `Coord` consists of a pair of `int32` elements `(q, r)`. \
The operators `==`, `!=`, `+`, `-` are defined. \
A *Spatial Hashing* method `CoordHash` is implemented for saving hashed version of the coordinates. This methodis used by `coordHash` elements into `BoardAsUnorderedMap` board implementation.

The directions are stored clockwise, starting from the `East` direction. This is needed by Mzinga engine for chosing the coordinates when placing a tile.

A method `neighborDirectionIndex` retrives the direction that links two tiles at their given coordinates `a` and `b`. \
A method `neighborAdjacent` returns the two adjacent neighbors of two adjacent tiles given their two coordinates `a` and `b`. 


### 3.3 The Engines
Currently, the engines implemented are the following:
- `RandomEngine`: randomly chooses a move among a given set of `validMoves`

Notice how the current approach of the engines always consists in chose among a set of given possible moves the "best" one. \
This implies, e.g., that the best move is currently affected by a (small) overhead since before the call of the valid moves generator is performed. \
To change this behavior, please modify the `getBestMove` call in `UhpHandler::cmdBestMove` call at line 228 in [uhp.cpp](./src/uhp.cpp#L228). \
For example, the call at [line 218](./src/uhp.cpp#L218) at the `generateMoves` method can be removed or applied only in certain cases.

#### 3.3.1 The Random Engine
The `RandomEngine` randomly chooses a move among a list of valid moves.

The method chacaterizing the engine is `RandomEngine::getBestMove`. \
It accepts a `Board`, a `Color` indicating the current player, a vector containing its `hand` and a vector containing the `validMoves`. \
Notice how the validity of the moves is never checked.
The method at first checks whether the `validMoves` is empty or not; if so, it chooses a `Move::Pass` move, filling the move with placeholder tags as `Move.piece.Color = White, Move.piece.Bug = Ant, Move.piece.id = 0, Move.from = {0, 0}, Move.to = {0, 0}` and returns.
Then, the method simply uses a non-deterministic seed to extract an element from the `validMoves` non-empty vector and returns that element.


### 3.4 The Generator of the moves
This is probably the most delicate file of the bot. \
It implements the methods for a class `MoveGenerator`:
- `generateMoves` : is the only public method. Simply calls `generatePlacements` and `generateMovements`, merges the vectors provided in output by them and returns the merged vector.
- `generatePlacements` : is a private method that retrieves all the possible valid moves of type `Move::Place` and returns them into a vector, given a `State`.
- `generateMovements` : is a private method retrieving all the possible valid moves of type `Move::PieceMove` and returns them into a vector, given a `State`.

1. `generatePlacements` \
   At first, it retrieves the current player, the current player's turn and the current board from `state`. \
   Calls then `state.getUniqueAvailablePieces` define at line 90 in [state.h](src/headers/state.h#L90), that returns a vector containing all the pieces in the hand of the current player. \
   The second step consists in checking for the Queen Placement Rule: the queen must be played before the forth turn of each player (i.e. at most at plies 6 and 7), but not at the first turn of each player (i.e. later then plies 1 and 2). \
   If the total amount of possible pieces to place is null, then the function will return the **empty** `placements` vector. \
   The forth step consists into retrieving the coordinates where to put the pieces available for placing. This is the longest step of the method:
   - Check whether the board is empty: if so, then the only valid coordinate `validCoord` is the couple `(0, 0)`.
   - If the number of occupied cells is 1, then the bot is playing the second move of the game: it is okay to put a piece of the bot's color adjacent to the neighbor's tile.
   - Finally, if the number of cells is greater than 1, the piece to be placed must touch a bug of its own color: to operate rapidly, an unordered set of coordinates is kept.
   The last step consists simply in filling each placement element with moves of type `Move::Place`.
2. `generateMovements` \
   At first, it retrieves the current player and the current board from `state`. \
   The second step consists into enforcing the No Queen No Movement rule. \
   A set of extreme points in the board is then calculated using `RuleEngine::getArticulationPoints` defined at Line 126 of [rules.cpp](src/rules.cpp#126). This is useful for dealing only with bugs that are at the extremes of the board. \
   If the last moved piece is of the same color of the player, it means that the piece has been moved by an opponent's pillbug.
   For getting the possible moves, given the piece considered, a method `Moves::get{pieceBug}Moves` is called. Special attention is related tp the pillbug moves that can perform also `Move::Drag` moves. \
   Each possible `Move::PieceMove` and `Move::Drag` move is then included into the `movements` vector. 


### 3.5 The Moves
The file `moves.cpp` presents all the possible pieces moves.

1. `Ant` : a BFS is performed.
2. `Beetle` : the beetle can move on top of a neighbor tile if its height is equal to the height of the tile.
3. `Grassopher` : can move following only one direction.
4. `Ladybug` : moves 2 tiles on top of another piece and one down into an empty space.
5. `Mosquito` : acts as its neighbors. some rules are applied:
   - If mosquito is on top of the hive, it acts as a beetle
   - If a mosquito touches a mosquito are at the same level, the bug has no movement capabilities
   Since pieces like Beetles and Queens can provide the same targets, memory is then check and deduplicated.
6. `Pillbug` : pillbug can both move and drag other pieces. Regarding the moves, the function called is `getQueenMoves` being the same idea; regarding pillbug drags, the method finds valid pieces to drag (i.e. the neighbor has not just moved and can actually move), find the valid spots, and finally apply a cartesian combination `{validPieces x validDestinations}`
7. `Queen` : Simply moves in a valid neighbor cell
8. `Spider` : Goes 3 steps into DFS.


### 3.6 The Pieces
The file `pieces.h` defines the `Piece` structure. 

Each piece is a tuple with the following:
- `Color` : the color of the piece
- `Bug` : the type of the bug (Ant, Beetle, Grassopher, Ladybug, Mosquito, Pillbug, Queen, Spider)
- `id` : the ID of the bug, i.e. the number identifying a specific bug. 

For example, the second ant of the white player (`wA2`) has `piece <- (color = Color::White, bug = Bug::Ant, id = 2)`.

Three methods are implemented:
1. `colorName` : returns the string view `White` or the string view `Black` given a color `col`.
2. `bugName` : returs the string view of the bug (e.g. `Ant`) given a bug `bug`.
3. `rival` : returns the color of the opposite player of the one who is playing.


### 3.7 The Rules
The rules implemented into `rules.cpp` are the following:
- `canSlide` : returns whether a piece on top of a tile at a given hexagonal coordinate `from` can move to a tile at a given hexagonal coordinate `target`.
- `touchesColor` : returns whether putting a piece of a given `color` into a tile at a given hexagonal coordinate `target` lead to touching a piece of the same `color`. This enforces the rule of putting pieces is allowed only if the destination is adjacent to a piece of the same color.
- `touchesHive` : returns whether moving a piece on top of a tile at a given coordinate `from` to a given coordinate `target`. Prevents moving pieces to the "limitless nothingness".
- `getArticulationPoints` : returns the articulation points of a given `board` using the **Tarjan's Algorithm**.
- `canLiftPiece` : returns whether a piece can be moved from its hexagonal coordinate. Replaces the legacy method `isBoardConnected`.

Some legacy methods are kept (despite never being called by the UHP bot):
- `isBoardConnected` : returns whether removing a piece on top of a tile at a given coordinate `coord` breaks the One Hive Rule

More in the specific:
1. `canSlide` \
   The idea: a piece can move from a coordinate `from` to an **adjacent** coordinate `target` if the two "gates" (i.e. the two common neighbors of the coordinates `from` and `target`) have height less or equal to the one of the starting coordinate `from`. \
   The method accepts also an optional parameter `ignoreCoord` that is used in case is needed to not consider the piece currently in a tile at that coordinate for the calculation of the height: it is the case, for example, of a ladybug move, which cannot move "cycling" between its previous positions. \
   The steps of the method are the following : 
   - A list of the adjacent neighbors is retrieved using the `neighborAdjacent` method defined into `coords.h`.
   - A lambda function for retrieving the height of the tiles at coordinates `from` and `to` is implemented. It removes 1 to the height if the coordinate is the one to ignore `ignoreCoord`, while adds 1 if the coordinate is where actually the piece *is* standing.
   - The calculation of the height of the `from` and the `target` coordinate is performed. Only the max between the two is kept.
   - The calculation of the height of the two `gates` is performed.
   - The method returns `false` if both the gates have height higher than the height of the maximum between the `from` and the `target` coordinate.
2. `touchesColor` \
   Just a simple for loop on the neighbors of the given hexagonal coordinate `coord` to check whether one of the neighbors have the preferred given `color`. 
3. `touchesHive` \
   Simply checks whether moving a piece from a given hexagonal coordinate `from` to a given hexagonal coordinate `target` leads to the piece touching some other piece in the `target` neighborhoods. \
   ! This method does not enforce the One Hive Rule !
4. `getArticulationPoints` \
   Implements the articulation points using the [Tarjan's Algorithm](https://www.geeksforgeeks.org/dsa/tarjan-algorithm-find-strongly-connected-components/) for finding the Strongly Connected Components using a DFS.
   - A list of all the occupied coordinates is retrieved by using `board.occupiedCoords`.
   - If the list contains only one (or nothing) vertices, the method return an empty set
   - The vertices coordinates are mapped using the `CoordHash` mapping.
   - A list of adjacenty is build and stored into a vector.
   - The DFS is performed. If a visited node is a root, with more then 1 child, it is saved as an articulation point. If a visited node is non-root and its discovery time is higher than the low value of one of its neighbors, it is saved as an articulation point.
   - The found articulation points are then mapped back to a set of meaningful coordinates (i.e. not hashed).
   - The articulation points set is returned.
   Notice how the DFS is performed only on the occupied coordinates, meaning that the complexity of the algorithm is $\mathcal{O}(V + E)$ where $V$ is the number of occupied coordinates and $E$ is the number of edges between them.
5. `canLiftPiece` \
   A piece can only leave its tile if: (i) the height of the tile is more than 1, meaning that the piece have some other bugs under it or (ii) the piece is not an articulation point. \
   A simple check on the height and on the presence in the set of the articulation points is performed.


### 3.8 The State
The state is another very delicate element of the UHP bot. It provides to all the backend functions a board, the players' hands, the history of the game plus some other information.
It defines also several methods useful to efficiently manage the hand, the application of the moves and the *undo* operation of the last played move.

The `State` class is defined as follows:
- A `Board`, of type `BoardAsArray` or `BoardAsUnorderedMap` dependently to the operations done at `board.h` file (see [Section 3.1](#31-the-board)).
- A `currentPlayer` of type `Color`.
- A `currentPlayerTurn`, indicating the total number of plies played by the two players.
- Two arrays of fixed size containing the hands of the two players.
- Two booleans for checking whether the Queen of the two colors have been played.
- A `coord` containing the coordinate of the last moved piece; this is needed for the undo operation and for pillbug's (and Mosquito's, if it plays as a pillbug) drag move.
- A vector of `HistoryStep`s containing the `history` of the game.

An `HistoryStep` is build as follows:
- A `Move`, with its own features (type, bug, coordinates).
- An `index` of the placed piece.
- The previous last moved piece coordinate (`previousLastMovedPieceCoord`).
- The previous `whiteQueenPlaced` and `blackQueenPlaced` parameters.

The class `State`, as told, exposes some methods:
1. `board` :  returns the `board`
2. `toMove` : returns the player that currently has to move
3. `getCurrentPlayerTurn` : returns the ply of the current player
4. `isQueenPlaced` : returns whether the queen of the given `color` has been played
5. `lastMovedPieceCoord ` : returns the coordinates of the last moved piece
6. `hasInHand` : returns whether the hand of the `piece.color` color contains the given `piece`.
7. `getUniqueAvailablePieces` : retrives the unique `bug` type (without the `id`) of the available pieces in hand of the player of the given `color`.
8. `applyMove` : applies a given `move` and updates the history and the `state` parameters. \
   If the move is a *placement*, the `_{color}QueenPlaced` parameter is updated. \
   If the move is a *movement* or a *drag* the `board.move` method is invoked.
9. `undoLastMove` : uses the `lastMovedPieceCoord` as a target coordinate for moving the last moved piece (found in the `_history`). The other `state` parameters are updated.


### 3.9 The UHP Protocol
The `uhp.cpp` and `utils.cpp` files represent the first layer between the entry point of the bot to the backend structure. 

#### 3.9.1 The UHP Loop
The `uhp.cpp` files is intended to extract the Mzinga referee commands and translate them into effective instructions to the backend.

The commands supported are the following:
1. `u1` \
   Is the simple "check" whether the engine is ready or not. \
   Prints "ok" into the `standard output`.
2. `info` \
   Is the initial "greeting" between the players. \
   Prints the `id name version` of the bot, the expansions supported (`Mosquito;Ladybug;Pillbug`) and "ok".
3. `newgame` \
   Starts a new game, possibly applying some given moves to the state.
4. `play` \
   Sends the given move to the `state` and consequently to the backend architecture.
5. `pass` \
   Applies a pass move.
6. `validmoves` \
   Retrieves all the possible valid moves in the current specific situation.
7. `bestmove` \
   Computes the best move in the current specific situation given an amount of time (for the used referee, `time 00:00:05` is used as the only constraint).
8. `undo` \
   Undoes the last move.
9. `options` \
   Currently not supported yet.

The UHP commands are never processed by the `uhp.cpp` file, but respective methods are called and calculations are performed by the backend structure.

#### 3.9.2 The UHP Utils
The `utils.cpp` file presents a list of utility functions that, more than everything, deal with the translation of a move of type `move` to an actual UHP move.

The majority of the worked is computed by the `UhpCodec` class, that manages internally the `state` of the game and the `UhpBoard`; it also provide methods to convert moves to Uhp strings.
A `UhpBoard` defines instead simple methods for dealing with an Uhp-compliant version of the board. The concept why it exists is a bit tricky, but the quick answer is that a board is needed to provide the correct Uhp strings for the moves.

Regarding the `UhpBoard`, it consists of an unordered map of stacks of hashed coordinates containing each pieces written following the UHP-compliant structure, and an inverse unordered map mapping each piece to its coordinate.
`UhpBoard` also provides the following private methods:
- `maxIndexForBug` : returns the maximum index for each bug type.
- `basePieceString` : maps each bug type to a specific string letter.
And some public methods:
- `occupied` : returns whether a given hexagonal coordinate is occupied.
- `topName` : returns the string of the piece on top of the UhpBoard.
- `hasPiece` : returns whether the `piecePos` has contains a given `pieceName` string.
- `whereIs` :  retrieves the coordinates of the a given `pieceName` string.
- `push`, `pop`, `moveTop` : for playing the moves.
- `nextPieceName` : returns the lowest index of the unused pieces.

The `UhpCodec` instead offers:
- `parseRelativePositionToken` : UHP token to hexagonal coordinate
- `desToRelativeToken` : hexagonal coordinate to UHP token
- `parseUhpRequest` : string command to `ParsedRequest` struct
- `moveToUhpString` : move to UHP token


There also exist other utilities:
- `trim` : trims a string
- `split_ws` : splits a string on empty spaces
- `splitCommand` : splits a line into chunks
