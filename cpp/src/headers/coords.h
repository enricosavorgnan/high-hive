#pragma once

#include <cstdint>
#include <array>
#include <cassert>

// COORDINATES for the BOARD
// The implementation mainly follows the one by Daniele.
// Hexagonal Coordinates are used:
// given a hexagonal tile defined by a couple of coordinates (q, r), its relative neighbours are defined as follows:

//         (0, -1)      /      \    (+1, -1)
//         (-1, 0)     | (q, r) |   (+1, 0)
//         (-1, +1)     \      /    (0, +1)

namespace Hive {

// Coordinates Structure
    struct Coord {
        std::int32_t q = 0;
        std::int32_t r = 0;

        // ----- Operators -----
        friend bool operator == (const Coord& a, const Coord& b) {
            return a.q == b.q && a.r == b.r;
        }
        friend bool operator != (const Coord& a, const Coord& b) {
            return !(a==b);
        }
        friend Coord operator + (const Coord& a, const Coord& b) {
            return {a.q + b.q, a.r + b.r};
        }
        friend Coord operator - (const Coord& a, const Coord& b) {
            return {a.q - b.q, a.r - b.r};
        }
    };

    // Hash required if using unordered_map
    // As discussed in "board.h", unorder_map is not used in the Board implementation.
    // However, the struct below is kept for any occurrence
    struct CoordHash {
        std::size_t operator()(const Coord& coord) const noexcept {
            std::uint64_t q = static_cast<std::uint16_t>(coord.q) * 73856093u;
            std::uint64_t r = static_cast<std::uint16_t> (coord.r) * 19349663u;

            // left shifted XOR
            return std::size_t (q ^ (r << 1));
        }
    };

    // Hexagonal Neighbors Direction
    static constexpr std::array<Coord, 6> DIRECTIONS = {
        Coord{1, 0},    // East
        Coord{0, 1},    // South-East
        Coord{-1, 1},   // South-West
        Coord{-1, 0},   // West
        Coord{0, -1},   // North-West
        Coord{1, -1}    // North-East
    };

    // Method for retrieving the neighbors tiles given a tile coordinate.
    // Returns Neighbors given a coordinate
    inline std::array<Coord, 6> coordNeighbors(const Coord& coord) {
        return {
            coord + DIRECTIONS[0],
            coord + DIRECTIONS[1],
            coord + DIRECTIONS[2],
            coord + DIRECTIONS[3],
            coord + DIRECTIONS[4],
            coord + DIRECTIONS[5]
        };
    }

    // Method for retrieving the direction from A to B if A, B are neighbours.
    // Returns the direction or -1 if A, B are not neighbors
    inline int neighborDirectionIndex(const Coord& a, const Coord& b) {
        Coord c = b-a;
        for (int i = 0; i < 6; i++) {
            if (c == DIRECTIONS[i]) return i;
        }
        return -1;
    }

    // Method for retrieving the two common neighbors if two tiles A,B are adjacent.
    // Returns the couples of coordinates of the two common neighbors
    inline std::pair<Coord, Coord> neighborAdjacent(const Coord& a, const Coord& b) {
        int direction = neighborDirectionIndex(a, b);
        assert(direction != -1 && "Coordinates A and B must be adjacent");

        Coord left = a + DIRECTIONS[(direction+5)%6];
        Coord right = b + DIRECTIONS[(direction+1)%6];
        return {left, right};
    }

}