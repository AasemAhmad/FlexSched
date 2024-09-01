#include "Shared/GraphAlgorithms.hpp"

std::vector<Distance> dijkstra(const std::vector<std::vector<Edge>> &graph, NodeIndex src)
{
    int nb_nodes = graph.size();
    std::vector<Distance> len(nb_nodes, std::numeric_limits<Distance>::max());
    std::vector<bool> visited(nb_nodes, false);
    len[src] = 0;
    std::priority_queue<DistanceNodeIndex, std::vector<DistanceNodeIndex>, std::greater<DistanceNodeIndex>> pq;

    pq.push({0, src});

    while (!pq.empty())
    {
        auto [_, v] = pq.top();
        pq.pop();

        if (visited[v])
        {
            continue;
        }

        visited[v] = true;

        for (const auto &[u, weight] : graph[v])
        {
            if (!visited[u] && len[u] > len[v] + weight)
            {
                len[u] = len[v] + weight;
                pq.push({len[u], u});
            }
        }
    }
    return len;
}