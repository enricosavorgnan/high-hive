#include "headers/board.h"


namespace Hive {

    // ----- !!!!! -----
    // Board as Array Implementations
    // ----- !!!!! -----
    void BoardAsArray::getOccupiedNeighbors(const Coord coord, std::vector<Coord>& out) const {
        out.clear();
        const int centerIdx = AxToIndex(coord);

        for (int i = 0; i < 6; ++i) {
            const int neighborIdx = centerIdx + NEIGHBORS[i];
            if (!_grid[neighborIdx].empty()) {
                out.push_back(coord + DIRECTIONS[i]);
            }
        }
    }


    void BoardAsArray::place (const Coord coord, const Piece piece) {
        int idx = AxToIndex(coord);

        if (_grid[idx].empty()) {
            _occupied_coords.push_back(coord);
        }
        _grid[idx].push(piece);
    }

    Piece BoardAsArray::remove(const Coord coord) {
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

    void BoardAsArray::move(const Coord from, const Coord to) {
        const Piece piece = remove(from);
        place(to, piece);
    }



    // ----- !!!!! -----
    // Board as Unordered Map Implementations
    // ----- !!!!! -----
    void BoardAsUnorderedMap::place(const Coord coord, const Piece piece) {
        if (empty(coord)) {
            _occupied_coords.push_back(coord);
        }
        cells_[coord].push_back(piece);
    }

    Piece BoardAsUnorderedMap::remove(const Coord coord) {
        auto& st = cells_.at(coord);
        Piece p = st.back();
        st.pop_back();

        if (st.empty()) {
            cells_.erase(coord);

            for (size_t i = 0; i < _occupied_coords.size(); ++i) {
                if (_occupied_coords[i] == coord) {
                    _occupied_coords[i] = _occupied_coords.back();
                    _occupied_coords.pop_back();
                    break;
                }
            }
        }
        return p;
    }

    void BoardAsUnorderedMap::move(Coord from, Coord to) {
        Piece p = remove(from);
        place(to, p);
    }


    void BoardAsUnorderedMap::getOccupiedNeighbors(Coord coord, std::vector<Coord>& out) const {
        // Leverage the mathematical neighbor generator to query surrounding geometry
        for (const Coord& neighbor : coordNeighbors(coord)) {
            if (!empty(neighbor)) {
                out.push_back(neighbor);
            }
        }
    }




}