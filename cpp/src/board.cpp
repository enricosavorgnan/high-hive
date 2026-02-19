#include "headers/board.h"


namespace Hive {

    const std::array<int, 6> Board::NEIGHBORS = {1, -1, -BOARD_DIM, BOARD_DIM, -BOARD_DIM+1, BOARD_DIM-1};

    void Board::place (Coord coord, Piece piece) {
        int idx = AxToIndex(coord);

        if (_grid[idx].empty()) {
            _occupied_coords.push_back(coord);
        }
        _grid[idx].push(piece);
    }

    Piece Board::remove(Coord coord) {
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
        return piece;
    }

    void Board::move(Coord from, Coord to) {
        Piece piece = Board::remove(from);
        Board::place(to, piece);
    }

    void Board::getOccupiedNeighbors(Coord coord, std::vector<Coord>& out) {
        out.clear();
        int centerIdx = AxToIndex(coord);

        for (int i = 0; i < 6; ++i) {
            int neighborIdx = centerIdx + NEIGHBORS[i];
            if (!_grid[neighborIdx].empty()) {
                out.push_back(coord + DIRECTIONS[i]);
            }
        }
    }

}