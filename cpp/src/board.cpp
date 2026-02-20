#include "headers/board.h"


namespace Hive {

    void Board::place (Coord coord, Piece piece) {
        int idx = AxToIndex(coord);

        if (_grid[idx].empty()) {
            _occupied_coords.push_back(coord);
        }
        _grid[idx].push(piece);
    }

    Piece Board::remove(Coord coord) {
        const int idx = AxToIndex(coord);
        const Piece piece = _grid[idx].pop();

        if (_grid[idx].empty()) {
            for (size_t i = 0; i < _occupied_coords.size(); ++i) {
                if (_occupied_coords[i] == coord) {
                    _occupied_coords[i] = _occupied_coords.back();
                    _occupied_coords.pop_back();
                    break;
                }   
            }
        }
        return piece;
    }

    void Board::move(const Coord from, const Coord to) {
        const Piece piece = remove(from);
        place(to, piece);
    }

    void Board::getOccupiedNeighbors(const Coord coord, std::vector<Coord>& out) const {
        out.clear();
        const int centerIdx = AxToIndex(coord);

        for (int i = 0; i < 6; ++i) {
            const int neighborIdx = centerIdx + NEIGHBORS[i];
            if (!_grid[neighborIdx].empty()) {
                out.push_back(coord + DIRECTIONS[i]);
            }
        }
    }

}