#include "headers/generator.h"

namespace Hive {

    std::vector<Move> MoveGenerator::generatePlacements(const State& state) {
        std::vector<Move> placements;
        Color player = state.toMove();
        const Board& board = state.board();
        int ply = state.getCurrentPlayerTurn();

        // 1. Get the list of all the pieces that are in the current player's hand
        std::vector<Piece> availablePieces = state.getUniqueAvailablePieces(player);

        // 2. Enforce Queen Placement Rule
        // White's 4th turn is ply 6; Black's is ply 7.
        bool mustPlaceQueen = !state.isQueenPlaced(player) &&
                              ((player == Color::White && ply == 6) ||
                               (player == Color::Black && ply == 7));
        // If the Queen must be played, the only returned value is the Queen placement
        if (mustPlaceQueen) {
            // Filter to strictly return only the Queen
            std::erase_if(availablePieces, [](const Piece& piece) { return piece.bug != Bug::Queen; });
        }
        if (ply < 2) {
            std::erase_if(availablePieces, [](const Piece& piece) { return piece.bug == Bug::Queen; });
        }

        // 3. Check whether the current player's hand is empty
        if (availablePieces.empty()) return placements;

        // 4. Identify Legal Coordinates
        std::vector<Coord> validCoords;
        const auto& occupied = board.occupiedCoords();

        // First move of the game
        if (occupied.empty()) {
            validCoords.push_back(Coord{0, 0});
        }
        // Second move of the game: must touch the first piece
        else if (occupied.size() == 1) {
            auto neighbors = coordNeighbors(occupied[0]);
            validCoords.assign(neighbors.begin(), neighbors.end());
        }
        // Standard placements: must touch own color, must NOT touch opponent color
        else {
            std::unordered_set<Coord, CoordHash> evaluatedCoords;;
            for (const Coord& c : occupied) {
                // If the piece belongs to the current player, its empty neighbors are potential placements
                const Piece* topPiece = board.top(c);
                if (!topPiece || topPiece->color != player) continue;
                for (const Coord& n : coordNeighbors(c)) {
                    if (board.empty(n)) {
                        // Avoid adding duplicates
                        if (evaluatedCoords.insert(n).second) {
                            if (!RuleEngine::touchesColor(board, n, rival(player))) {
                                validCoords.push_back(n);
                            }
                        }
                    }
                }
            }
        }

        // 5. Map to Move Structs
        placements.reserve(availablePieces.size() * validCoords.size());
        for (const Piece& piece : availablePieces) {
            for (const Coord& coord : validCoords) {
                Move m;
                m.type = Move::Place;
                m.piece = piece;
                m.to = coord;
                placements.push_back(m);
            }
        }

        return placements;
    }


    std::vector<Move> MoveGenerator::generateMovements(const State& state) {
        std::vector<Move> movements;
        Color player = state.toMove();
        const Board& board = state.board();

        // No Queen No Movement Rule
        if (!state.isQueenPlaced(player)) return movements;

        movements.reserve(128);

        // Precompute articulation points O(V) once per game node
        std::unordered_set<Coord, CoordHash> articulationPoints = RuleEngine::getArticulationPoints(board);

        // Loop over the occupied coordinates into the board
        for (const Coord& origin : board.occupiedCoords()) {
            const Piece* topPiece = board.top(origin);
            if (!topPiece || topPiece->color != player) continue;
            // If the last moved piece belongs to the current player, it means that the opponent's Pillbug dragged it. It is paralyzed.
            if (state.lastMovedPieceCoord() && origin == *state.lastMovedPieceCoord()) continue;

            std::vector<Coord> normalTargets;
            std::vector<std::pair<Coord, Coord>> dragTargets;

            // O(1) mathematical One-Hive validation
            if (RuleEngine::canLiftPiece(board, origin, articulationPoints)) {
                switch (topPiece->bug) {
                    case Bug::Queen:       Moves::getQueenMoves(board, origin, normalTargets); break;
                    case Bug::Beetle:      Moves::getBeetleMoves(board, origin, normalTargets); break;
                    case Bug::Spider:      Moves::getSpiderMoves(board, origin, normalTargets); break;
                    case Bug::Grasshopper: Moves::getGrasshopperMoves(board, origin, normalTargets); break;
                    case Bug::Ant:         Moves::getAntMoves(board, origin, normalTargets); break;
                    case Bug::Ladybug:     Moves::getLadybugMoves(board, origin, normalTargets); break;
                    case Bug::Pillbug:     break;
                    case Bug::Mosquito:    Moves::getMosquitoMoves(board, origin, normalTargets, state.lastMovedPieceCoord(), dragTargets, articulationPoints); break;
                }
            }

            if (topPiece->bug == Bug::Pillbug) {
                Moves::getPillbugMoves(board, origin, normalTargets, state.lastMovedPieceCoord(), dragTargets, articulationPoints);
            }

            for (const Coord& target : normalTargets) {
                Move m;
                m.type = Move::PieceMove;
                m.piece = *topPiece;
                m.from = origin;
                m.to = target;
                movements.push_back(m);
            }

            for (const auto& dragPair : dragTargets) {
                Move m;
                m.type = Move::Drag;
                m.piece = *board.top(dragPair.first);
                m.from = dragPair.first;
                m.to = dragPair.second;
                m.pillbug = origin;
                movements.push_back(m);
            }
        }

        return movements;
    }

    std::vector<Move> MoveGenerator::generateMoves(const State& state) {
        std::vector<Move> placements = generatePlacements(state);

        std::vector<Move> movements = generateMovements(state);

        placements.insert(placements.end(), movements.begin(), movements.end());

        return placements;
    }
}
