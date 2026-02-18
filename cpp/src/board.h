#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <algorithm>
#include <cassert>

#include "coords.h"
#include "pieces.h"


namespace Hive {
    constexpr int BOARD_DIM = 64; // should include all the aligned pieces
    constexpr int BOARD_OFFSET = BOARD_DIM / 2;
    constexpr int BOARD_AREA = BOARD_DIM * BOARD_DIM;
    constexpr int MAX_STACK = 8; // to bound height of the cells

    // Board cell
    template <typename Piece, int N>
    class CellStack {
        public:
            std::array<Piece, N> _data;     // Piece
            uint8_t _count = 0;         // Number of elements < N
            
            // Utilities
            void push(const Piece& val) {
                assert(_count < N) && "Stack overflow: Piece stack too high";
                _data[_count++] = val;
            }

            Piece pop() {
                assert(_count > 0 && "Stack Underflow");
                return _data[--_count];
            }

            const Piece& top() const {
                assert(_count > 0);
                return _data[_count - 1];
            }

            bool contains(const Piece& val) const{
                for (int i = 0; i<_count; ++i) {
                    if (_data[i] == val) return true;
                }
                return false;
            }

            bool empty() const {
                return _count == 0;
            }
            int size() const {
                return _count;
            }
            void clear() {
                _count = 0;
            }

            // Auto for loops
            auto begin() {
                return _data.begin();
            }
            auto end() {
                return _data.begin() + _count;
            }
            auto begin const {
                return _data.begin();
            }
            auto end const {
                return _data.begin() + _count;
            }
    };

    // Board 
    // 1D array of dimension BOARD_AREA 
    // Tiles can be retrieved with indexes  

    class Board{
        friend class RuleEngine;
        
        public:
            using Cell = CellStack<Piece, MAX_STACK>;

        private:
            // Grid
            std::array<Cell, BOARD_AREA> _grid;

            // Coordinates
            std::vector<Coord> _occupied_coords;

            // Tile Neighbours
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
            Board() {
                _occupied_coords.reserve(32);   
            }


            // ----- Coordinates Math ----- 

            [[nodiscard]] static inline int AxToIndex(Coord coord) {
                assert(isValid(coord) && "Axial Coordinate is not valid");
                return (coord.r + BOARD_OFFSET) * BOARD_DIM  + (coord.q + BOARD_OFFSET);
            }

            [[nodiscard]] static inline bool isValid(Coord coord) {
                int q = coord.q + BOARD_OFFSET;
                int r = coord.r + BOARD_OFFSET;
                return q >= 0 && q < BOARD_DIM && r < BOARD_DIM;
            }


            // ----- Queries -----
            
            // Get top piece
            const Piece* top(Coord coord) const{
                const Cell& idx = _grid[AxToIndex(coord)];
                if (idx.empty()) return nullptr;
                return &idx.top();
            }

            // Get occupied cells 
            const std::vector<Coord>& occupiedCoords() const {
                return _occupied_coords;
            }
            
            // Get cell height
            int height (Coord coord) const {
                return _grid[AxToIndex(coord)].size();
            }

            // Is the cell empty
            bool empty(Coord coord) const {
                return _grid[AxToIndex(coord)].empty();
            }


            // ----- Operations -----

            void place (Coord coord, Piece piece) {
                int idx = AxToIndex(coord);

                if (_grid[idx].empty()) {
                    _occupied_coords.push_back(coord);
                }
                _grid[idx].push(piece);
            }

            Piece remove(Coord coord) {
                int idx = AxToIndex(coord);
                Piece piece = _grid[idx].pop();

                if (_grid[idx].empty()) {
                    for (size_t i = 0; i < _occupied_coords.size(); ++i) {
                        if (_occupied_coords[i] == coord) {
                            _occupied_coords[i] = _occupied_coords.back();
                            _occupied_coords.pop_back();
                            break;
                        }
                    }
                }
            }

            void move(Coord from, Coord to) {
                Piece piece = remove(from);
                place(to, piece);
            }

            void getOccupiedNeighbors(Coord coord, std::vector<Coord>& out) const {
                out.clear();
                int centerIdx = AxToIndex(coord);

                for (int i = 0; i < 6; ++i) {
                    int neighborIdx = centerIdx + NEIGHBORS[i];
                    if (!_grid[neighborIdx].empty()) {
                        out.push_back(coord + HEXGRID_DIR[i]);
                    }
                }
            }

            // Hive Move Rule
            // Move only if at least oneof the two gates is not blocked by a piece
            bool canSlide(Coord from, Coord to) const {
                int fromIdx = AxToIndex(from);
                int toIdx = AxToIndex(to);
                int diff = toIdx - fromIdx;

                int blockedGates = 0;

                for (int i = 0; i < 6; ++i) {
                    int neighborIdx = fromIdx + NEIGHBORS[i];

                    bool isCommon = false;
                    int dist = neighborIdx - toIdx;
                    for (int d : NEIGHBORS) {
                        if (dist == -d) {
                            isCommon = true;
                            break;
                        }
                    }

                    if (isCommon) {
                        if (!_grid[neighborIdx].empty()) {
                            blockedGates++;
                        }
                    }
                }

                return blockedGates < 2;
            }

    }

}