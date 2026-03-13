#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>

#include "coords.h"
#include "pieces.h"

// ----------------- !!!!!! -----------------
// Uncomment the following for using Unordered Map as active Board structure
//
// #define USE_UNORDERED_MAP_BOARD
//
// ----------------- !!!!!! -----------------





namespace Hive {

    // The following refers to Board as an Array.
    // Board as an Unordered is presented at line 197


    // CELL and BOARD IMPLEMENTATION
    // The board is implemented as a 1D array of dimension BOARD_AREA, where each cell is a stack of pieces (CellStack).
    // The CellStack is implemented as a fixed-size array with at most MAX_STACK pieces.

    // Constant values
    constexpr int BOARD_DIM = 64; // should include all the aligned pieces. Actually 55 should work
    constexpr int BOARD_OFFSET = BOARD_DIM / 2; // Offset for dealing with coordinates
    constexpr int BOARD_AREA = BOARD_DIM * BOARD_DIM; // Total grid area
    constexpr int MAX_STACK = 6; // To bound the height of the cells. Actually, heights > 4 are quite rare

    // CELL
    template <typename Piece, int N>

    // Each cell is implemented as a tuple[array, int],
    // where the array has size N in its general form, MAX_STACK passed by Board class in practice
    class CellStack {
        public:
            std::array<Piece, N> _data;     // Piece(s)
            uint8_t _count = 0;             // Number of elements < N
            
            // ----- Utilities -----
            // Push a value inside the Stack
            void push(const Piece& val) {
                assert(_count < N && "Stack overflow: Piece stack too high");
                _data[_count++] = val;
            }

            // Pop a value from the Stack
            Piece pop() {
                assert(_count > 0 && "Stack Underflow");
                return _data[--_count];
            }

            // Get the heighest top in the stack
            [[nodiscard]] const Piece& top() const {
                assert(_count > 0);
                return _data[_count - 1];
            }

            // Returns True if val is inside the Cell
            [[nodiscard]] bool contains(const Piece& val) const{
                for (int i = 0; i<_count; ++i) {
                    if (_data[i] == val) return true;
                }
                return false;
            }

            // Returns True if the Cell is empty
            [[nodiscard]] bool empty() const {
                return _count == 0;
            }
            // Returns the number of elements in the Cell
            [[nodiscard]] int height() const {
                return _count;
            }
            // Clear the Cell
            void clear() {
                for (int i = 0; i<_count; ++i) _data[i] = Piece();
                _count = 0;
            }

            // Auto for loops
            auto begin() {
                return _data.begin();
            }
            auto end() {
                return _data.begin() + _count;
            }
            [[nodiscard]] auto begin() const {
                return _data.begin();
            }
            [[nodiscard]] auto end() const {
                return _data.begin() + _count;
            }
    };




    // BOARD
    // 1D array of dimension BOARD_AREA
    // The first placed piece gets coordinates (q=0, r=0) --> (q+BOARD_OFFSET, r+BOARD_OFFSET) in the grid, and then "spliced" as follows:
    // (q, r) -> q+BOARD_OFFSET + (r+BOARD_OFFSET)*BOARD_DIM
    // A vector _occuied_cells takes care of all the cells with pieces above
    class BoardAsArray{
        friend class RuleEngine;

        public:
            using Cell = CellStack<Piece, MAX_STACK>;


        private:
            // Grid
            std::array<Cell, BOARD_AREA> _grid;
            // Occupied Coordinates
            std::vector<Coord> _occupied_coords;

            // Tile Neighbors
            // Is the (negative) difference between a hypothetical piece (q, r) and its neighbors
            static constexpr std::array <int, 6> NEIGHBORS = {
                1,                      // (1, 0)
                BOARD_DIM,              // (0, 1)
                BOARD_DIM - 1,          // (-1, 1)
                -1,                     // (-1, 0)
                -BOARD_DIM,             // (0, -1)
                -BOARD_DIM + 1          // (1, -1)
            };

        public:
            // Reserve memory for each of the 28 cells
            BoardAsArray() {
            _occupied_coords.reserve(32);
            }


            // ----- Coordinates Math -----
            [[nodiscard]] static int AxToIndex(Coord coord) {
                assert(isValid(coord) && "Axial Coordinate is not valid");
                return (coord.r + BOARD_OFFSET) * BOARD_DIM  + (coord.q + BOARD_OFFSET);
            }

            [[nodiscard]] static bool isValid(Coord coord) {
                int q = coord.q + BOARD_OFFSET;
                int r = coord.r + BOARD_OFFSET;
                return q >= 0 && q < BOARD_DIM && r >= 0 && r < BOARD_DIM;
            }


            // ----- Queries -----

            // Get occupied cells
            [[nodiscard]] const std::vector<Coord>& occupiedCoords() const {
                    return _occupied_coords;
            }

            // Retrieve all the occupied cells neighbor to a given coordinate
            void getOccupiedNeighbors(Coord coord, std::vector<Coord>& out) const;

            // Get top piece over a given Coordinate
            [[nodiscard]] const Piece* top(const Coord coord) const{
                const Cell& idx = _grid[AxToIndex(coord)];
                if (idx.empty()) return nullptr;
                return &idx.top();
            }
            
            // Get cell height
            [[nodiscard]] int height (Coord coord) const {
                return _grid[AxToIndex(coord)].height();
            }

            // Is the cell empty
            [[nodiscard]] bool empty(Coord coord) const {
                return _grid[AxToIndex(coord)].empty();
            }


            // ----- Operations -----

            void place (Coord coord, Piece piece);

            Piece remove(Coord coord);

            void move(Coord from, Coord to);
    };



    class BoardAsUnorderedMap {
    public:
        using Stack = std::vector<Piece>;
        using UPCellStack = std::vector<Piece>;

    private:
        std::unordered_map<Coord, UPCellStack, CoordHash> cells_;
        std::vector<Coord> _occupied_coords;

    public:
        //l'immagine evocativa è che moralmente è una scacchiera di stack di pezzi
        //il primo pezzo che abbandona una cella è quello posto più in alto
        //finchè quello più in alto non si sposta, quelli sotto non si spostano


        BoardAsUnorderedMap() {
            _occupied_coords.reserve(32);
        }

        // ----- Queries -----
        const Piece* top(const Coord coord) const {
            auto it = cells_.find(coord);
            if (it == cells_.end() || it->second.empty()) return nullptr;
            return &it->second.back();
        }

        const std::vector<Coord>& occupiedCoords() const {
            return _occupied_coords;
        }


        int height(const Coord coord) const {
            auto it = cells_.find(coord);
            return (it == cells_.end()) ? 0 : static_cast<int>(it->second.size());
        }

        bool empty (Coord coord) const {
            auto it = cells_.find(coord);
            return it == cells_.end() || it->second.empty();
        }



        // ----- Operations -----
        void place(Coord coord, Piece piece);

        Piece remove(Coord coord);

        void move(Coord from, Coord to);

        void getOccupiedNeighbors(Coord coord, std::vector<Coord>& out) const;


        // LEGACY FUNCTIONS

        // void push(const Coord& c, const Piece& p) {
        //     cells_[c].push_back(p);
        // }
        //
        // Piece pop(const Coord& c) {
        //     auto& st = cells_.at(c);
        //     Piece p = st.back();
        //     st.pop_back();
        //     if (st.empty()) cells_.erase(c);
        //     return p;
        // }
        //
        // void moveTop(const Coord& from, const Coord& to) {
        //     Piece p = pop(from);
        //     push(to, p);
        // }

        // std::vector<Coord> occupiedCells() const {
        //     std::vector<Coord> out;
        //     //ci serve che il vettore out delle celle abbia posto per esattamente tante coordinate quanti gli elementi di cells
        //     out.reserve(cells_.size());
        //     //ciclo for per iterare in maniera più semplice su tutte le componenti di cells, kv=key,value
        //     for (const auto& kv : cells_) out.push_back(kv.first);
        //     return out;
        // }
    };

#ifdef USE_UNORDERED_MAP_BOARD
    using Board = BoardAsUnorderedMap;
#else
    using Board = BoardAsArray;
#endif
};