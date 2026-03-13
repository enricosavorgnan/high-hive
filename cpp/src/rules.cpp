#include "headers/rules.h"
#include <bitset>
#include <array>

namespace Hive{

    // Keep physical contact
    bool RuleEngine::touchesHive(const Board& board, Coord target, Coord prop) {
        // If we are leaving a stack, the underlying piece remains a valid hive connection
        bool propRemainsOccupied = (board.height(prop) > 1);

        for (const auto& n : coordNeighbors(target)) {
            if (n == prop) {
                if (propRemainsOccupied) return true;
                continue;
            }
            if (!board.empty(n)) return true;
        }
        return false;
    }

    bool RuleEngine::canSlide(const Board& board, Coord from, Coord to, std::optional<Coord> ignoreProp) {
        // Retrieve the two common adjacent hexagonal "gates" between 'from' and 'to'
        std::pair<Coord, Coord> common = neighborAdjacent(from, to);

        // Lambda function needed for LadyBug moves
        auto vHeight = [&](Coord c, bool isCurrentlyHere) {
            int h = board.height(c);
            if (c == ignoreProp && ignoreProp.has_value() ) h -= 1; // The bug physically left its origin
            if (isCurrentlyHere) h += 1; // The bug is currently standing here
            return h;
        };

        // 3D Sliding Check: Retrieve the stack heights using the public interface
        int hFrom = vHeight(from, true);
        int hTo = vHeight(to, false);

        // Calculate the peak transition height
        int maxHeight = std::max(hFrom, hTo + 1);

        int hGate1 = vHeight(common.first, false);
        int hGate2 = vHeight(common.second, false);

        // The slide is blocked if BOTH gates are at or above the maximum transition height
        return !(hGate1 >= maxHeight && hGate2 >= maxHeight);
    }

    bool RuleEngine::isBoardConnected(const Board& board, Coord coord) {
        // Stack check: If the stack height is >= 2, removing the top piece leaves a piece behind:
        // current connectivity is maintained.
        if (board.height(coord) >= 2) {
            return true;
        }

        // Identify Occupied neighbors
        std::array<Coord, 6> occNeighbors;
        int neighCount = 0;

        for (const Coord& n : coordNeighbors(coord)) {
            if (!board.empty(n)) {
                occNeighbors[neighCount++] = n;
            }
        }

        // Leaf node check: A node with 0 or 1 neighbors is not an articulation point:
        // current connectivity is maintained.
        if (neighCount < 2) {
            return true;
        }

        // BFS traversal to verify if all neighbors remain connected without the piece at 'coord'.
        // Note: For N <= 28, a pre-allocated vector with std::find is faster than hashing in an unordered_set.
        std::vector<Coord> visited;
        visited.reserve(32);

        std::vector<Coord> q;
        q.reserve(32);

        // Initialize BFS from the first discovered occupied neighbor
        q.push_back(occNeighbors[0]);
        visited.push_back(occNeighbors[0]);

        size_t head = 0;
        while (head < q.size()) {
            Coord curr = q[head++];

            for (const Coord& next : coordNeighbors(curr)) {
                // Strictly exclude the piece being simulated for removal, and empty cells.
                if (next == coord || board.empty(next)) {
                    continue;
                }

                // Check if the coordinate has already been evaluated
                if (std::find(visited.begin(), visited.end(), next) != visited.end()) {
                    continue;
                }

                visited.push_back(next);
                q.push_back(next);
            }
        }

        // Connectivity Validation: If any original neighbor is absent from the visited set,
        // it implies the graph has been partitioned into distinct components.
        for (int i = 1; i < neighCount; ++i) {
            if (std::find(visited.begin(), visited.end(), occNeighbors[i]) == visited.end()) {
                return false;
            }
        }

        return true;
    }


    std::unordered_set<Coord, CoordHash> RuleEngine::getArticulationPoints(const Board& board) {
        const std::vector<Coord>& verts = board.occupiedCoords();
        int n = static_cast<int>(verts.size());

        std::unordered_set<Coord, CoordHash> ap_set;
        if (n <= 1) return ap_set;

        // 1. Map Coordinates to 1D array indices [0...n-1] for the DFS algorithm
        std::unordered_map<Coord, int, CoordHash> id_map;
        id_map.reserve(n * 2);
        for (int i = 0; i < n; ++i) {
            id_map[verts[i]] = i;
        }

        // 2. Build the Adjacency List
        std::vector<std::vector<int>> adj(n);
        for (int i = 0; i < n; ++i) {
            for (const Coord& neighbor : coordNeighbors(verts[i])) {
                if (!board.empty(neighbor)) {
                    adj[i].push_back(id_map[neighbor]);
                }
            }
        }

        // 3. Tarjan's DFS State Variables
        std::vector<int> disc(n, -1);
        std::vector<int> low(n, -1);
        std::vector<int> parent(n, -1);
        std::vector<bool> ap(n, false);
        int time = 0;

        // DFS Lambda
        std::function<void(int)> dfs = [&](int u) {
            disc[u] = low[u] = ++time;
            int children = 0;

            for (int v : adj[u]) {
                if (disc[v] == -1) { // If v is not visited
                    children++;
                    parent[v] = u;
                    dfs(v);

                    low[u] = std::min(low[u], low[v]);

                    // Case 1: Root node with >= 2 independent children
                    if (parent[u] == -1 && children > 1) ap[u] = true;

                    // Case 2: Non-root node where no back-edge reaches above u
                    if (parent[u] != -1 && low[v] >= disc[u]) ap[u] = true;

                } else if (v != parent[u]) { // Back-edge found
                    low[u] = std::min(low[u], disc[v]);
                }
            }
        };

        // 4. Execute DFS
        for (int i = 0; i < n; ++i) {
            if (disc[i] == -1) dfs(i);
        }

        // 5. Translate articulation indices back to true Coords
        for (int i = 0; i < n; ++i) {
            if (ap[i]) ap_set.insert(verts[i]);
        }

        return ap_set;
    }


    bool RuleEngine::canLiftPiece(const Board& board, Coord coord, const std::unordered_set<Coord, CoordHash>& articulationPoints) {
        // If it is stacked, lifting the top piece leaves a physical piece behind, preserving the hive.
        if (board.height(coord) >= 2) return true;

        // If it is NOT stacked, it must NOT be an articulation point.
        return articulationPoints.find(coord) == articulationPoints.end();
    }
}