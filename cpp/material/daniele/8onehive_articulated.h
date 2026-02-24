#ifndef HIVE_CORE_ONEHIVE_H
#define HIVE_CORE_ONEHIVE_H

#include "2hexgrid.h"
#include "4board.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hive::core {

// Articulation points del grafo "celle occupate".
// Vertici: coordinate occupate (una per cella, indipendente dall'altezza dello stack)
// Archi: adiacenza esagonale fra celle occupate
inline std::unordered_set<Coord, CoordHash> oneHiveArticulationPoints(const Board& b) {
  const std::vector<Coord> verts = b.occupiedCells();
  const int n = static_cast<int>(verts.size());

  std::unordered_set<Coord, CoordHash> out;
  if (n == 0) return out;

  //etichetta per ogni coppia di coordinate occupate: id[coord] -> indice [0..n)
  std::unordered_map<Coord, int, CoordHash> id;
  id.reserve(static_cast<std::size_t>(n) * 2);
  for (int i = 0; i < n; ++i) id.emplace(verts[i], i);

  // adjacency list
  std::vector<std::vector<int>> adj(n);
  for (int i = 0; i < n; ++i) {
    adj[i].reserve(6);
    for (const auto& nb : hexgridNeighbors(verts[i])) {
      auto it = id.find(nb);
      if (it != id.end()) adj[i].push_back(it->second);
    }
  }

  // Tarjan: disc/low
  std::vector<int> disc(n, -1); //tempo di visita, quando è stato visitato per la prima volta
  std::vector<int> low(n, -1); //
  std::vector<int> parent(n, -1); //il nodo genitore nell'albero della dfs
  std::vector<char> ap(n, 0); //alla fine se 1 vuold ire che è un articulation point
  int timer = 0;

  std::function<void(int)> dfs = [&](int u) {
    disc[u] = low[u] = timer++;
    int children = 0;

    for (int v : adj[u]) {
      if (disc[v] == -1) {
        parent[v] = u;
        ++children;
        dfs(v);

        low[u] = std::min(low[u], low[v]);

        // Caso 1: root con >=2 figli
        if (parent[u] == -1 && children > 1) ap[u] = 1;

        // Caso 2: non-root, nessun back-edge dal sottoalbero di v che risale sopra u
        if (parent[u] != -1 && low[v] >= disc[u]) ap[u] = 1;

      } else if (v != parent[u]) {
        // back-edge
        low[u] = std::min(low[u], disc[v]);
      }
    }
  };

  // Se per qualche motivo lo stato non è connesso, copriamo tutte le componenti, anche se per hive non dovrebbe succedere.
  for (int i = 0; i < n; ++i) {
    if (disc[i] == -1) dfs(i);
  }

  out.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    if (ap[i]) out.insert(verts[i]);
  }
  return out;
}

// Check ONE-HIVE durante uno spostamento: la pedina viene "sollevata" dalla cella `from`.
// Se lo stack ha altezza >= 2, la cella resta comunque occupata => non può spezzare l'alveare.
// Se altezza == 1, allora la cella sparisce dal grafo, e serve che `from` NON sia articulation point.
inline bool oneHiveAllowsLiftFrom(const Board& b,
                                 const Coord& from,
                                 const std::unordered_set<Coord, CoordHash>& articulation) {
  if (!b.occupied(from)) return true;   // non dovrebbe accadere in movegen
  if (b.height(from) >= 2) return true; // resta un pezzo sotto
  return articulation.find(from) == articulation.end();
}

} // namespace hive::core

#endif