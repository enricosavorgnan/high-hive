#pragma once

#include <cstdint>
#include <array>
#include <cassert>

namespace Hive {

// Hexagonal coordinates
struct Coord {
    std::int16_t q = 0;
    std::int16_t r = 0;

    // operators definitions 
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

// hash required for custom unordered_map
struct CoordHash {
    std::size_t operator()(const Coord& coord) const noexcept {
        std::uint64_t q = static_cast<std::uint16_t>(coord.q) * 73856093u;
        std::uint64_t r = static_cast<std::uint16_t> (coord.r) * 19349663u;

        // left shifted XOR 
        return std::size_t (q ^ (r << 1));
    }
};

// hexagonal grid
static constexpr std::array<Coord, 6> HEXGRID_DIR = {
    Coord{1, 0},    // East
    Coord{0, 1},    // South-East
    Coord{-1, 1},   // South-West
    Coord{-1, 0},   // West
    Coord{0, -1},   // North-West
    Coord{1, -1}    // North-East
};

// returns 6 neighbors of a given coordinate
inline std::array<Coord, 6> coordNeighbors(const Coord& coord) {
    return { 
        coord + HEXGRID_DIR[0], 
        coord + HEXGRID_DIR[1], 
        coord + HEXGRID_DIR[2], 
        coord + HEXGRID_DIR[3], 
        coord + HEXGRID_DIR[4], 
        coord + HEXGRID_DIR[5] 
    };
}

// returns index of direction from A to B, otherwise -1
inline int neighborDirectionIndex(const Coord& a, const Coord& b) {
    Coord c = b-a;
    for (int i = 0; i < 6; i++) {
        if (c == HEXGRID_DIR[i]) return i;
    }
    return -1;
}

inline std::pair<Coord, Coord> neighborAdjacent(const Coord& a, const Coord& b) {
    int direction = neighborDirectionIndex(a, b);
    assert(direction != -1 && "Coordinates A and B must be adjacent");

    Coord left = a + HEXGRID_DIR[(direction+5)%6];
    Coord right = b + HEXGRID_DIR[(direction+1)%6];
    return {left, right};
}

// returns 

}