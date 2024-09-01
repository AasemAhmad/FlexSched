#include <iostream>
#include <limits>
#include <queue>
#include <vector>

using NodeIndex = int;
using Weight = int;
using Distance = int;

using Edge = std::pair<NodeIndex, Weight>;
using DistanceNodeIndex = std::pair<Distance, NodeIndex>;

std::vector<Distance> dijkstra(const std::vector<std::vector<Edge>> &graph, NodeIndex src);
