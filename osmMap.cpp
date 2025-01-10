#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <cmath>
#include <queue>
#include <functional>
#include <vector>
#include <unordered_set>
#include <limits>
#include <utility>
#include<unordered_map>
#include "osmMap.h"
#include "quadTree.h"
#include "tinyxml/tinyxml.h"
#include "jsoncpp/json/json.h"

osmMap::osmMap() {
    loadFromJson();
    buildnodeTree();
}

void osmMap::buildnodeTree(){
    drivingNodeTree = QuadTree(30.6533,120.6148,31.4486,122.1117);
    cyclingNodeTree = QuadTree(30.6533,120.6148,31.4486,122.1117);
    walkingNodeTree = QuadTree(30.6533,120.6148,31.4486,122.1117);
    for (const auto& node : drivingAdjacencyList) {
        const std::string& id = node.first;
        double lat = nodes[id].first;
        double lon = nodes[id].second;
        Node* newNode = new Node(lat, lon, id);  // 创建节点对象
        drivingNodeTree.insert(newNode);  // 插入到四叉树中
    }
    std::cout << "DrivingQuadTree built successfully\n";
    for (const auto& node : cyclingAdjacencyList) {
        const std::string& id = node.first;
        double lat = nodes[id].first;
        double lon = nodes[id].second;
        Node* newNode = new Node(lat, lon, id);  // 创建节点对象
        cyclingNodeTree.insert(newNode);  // 插入到四叉树中
    }
    std::cout << "CyclingQuadTree built successfully\n";
    for (const auto& node : walkingAdjacencyList) {
        const std::string& id = node.first;
        double lat = nodes[id].first;
        double lon = nodes[id].second;
        Node* newNode = new Node(lat, lon, id);  // 创建节点对象
        walkingNodeTree.insert(newNode);  // 插入到四叉树中
    }
    std::cout << "WalkingQuadTree built successfully\n";
}

std::string osmMap::fetchNearestNode(double lat, double lon ,std::string mode = "driving"){
    auto& nodeTree =
    (mode == "driving") ? drivingNodeTree :
    (mode == "walking") ? walkingNodeTree : cyclingNodeTree;

    return nodeTree.queryNearest(lat,lon) -> ID;

}


void osmMap::loadFromJson(const std::string& inputFilePath) {
    std::ifstream inFile(inputFilePath, std::ios::in);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open JSON file: " << inputFilePath << std::endl;
        return;
    }

    Json::CharReaderBuilder reader;
    Json::Value root;
    std::string errs;

    if (!Json::parseFromStream(reader, inFile, &root, &errs)) {
        std::cerr << "Failed to parse JSON file: " << errs << std::endl;
        return;
    }

    // 加载节点和路径信息
    auto nodesJson = root["nodes"];
    // 遍历 "nodesJson" 中的每个元素
    for (const auto& key : nodesJson.getMemberNames()) {
        const Json::Value& node = nodesJson[key];

        // 提取每个节点的 lat, lon
        double lat = std::stod(node["lat"].asString());  // 或者直接用 node["lat"].asDouble()
        double lon = std::stod(node["lon"].asString());  // 或者直接用 node["lon"].asDouble()

        // 将节点存入 unordered_map，存储 id 和 lat-lon 对
        nodes[key] = {lat,lon};
    }

    // 通用函数：加载邻接表
    auto loadAdjacencyList = [](const Json::Value& adjacencyJson) {
        std::unordered_map<std::string, std::unordered_map<std::string, double>> adjacencyList;
        for (const auto& key : adjacencyJson.getMemberNames()) {
            const Json::Value& neighborsJson = adjacencyJson[key];
            std::unordered_map<std::string, double> neighbors;
            for (const auto& neighbor : neighborsJson) {
                std::string neighborNode = neighbor["node"].asString();
                double distance = neighbor["distance"].asDouble();
                neighbors[neighborNode] = distance;
            }
            adjacencyList[key] = neighbors;
        }
        return adjacencyList;
    };

    // 加载不同模式的邻接表
    if (root["adjacencyLists"].isMember("driving")) {
        drivingAdjacencyList = loadAdjacencyList(root["adjacencyLists"]["driving"]);
    }
    if (root["adjacencyLists"].isMember("walking")) {
        walkingAdjacencyList = loadAdjacencyList(root["adjacencyLists"]["walking"]);
    }
    if (root["adjacencyLists"].isMember("cycling")) {
        cyclingAdjacencyList = loadAdjacencyList(root["adjacencyLists"]["cycling"]);
    }

    std::cout << "Nodes, ways, and adjacency lists loaded from " << inputFilePath << std::endl;
}




Json::Value osmMap::findShortestPath(const std::string& startID, const std::string& endID ,const std::string& mode = "driving", const std::string& algorithm = "A*"){
    if(algorithm == "A*") return AStarSearch(startID,endID,mode);
    if(algorithm == "Dijkstra") return Dijkstra(startID,endID,mode);
}

Json::Value osmMap::AStarSearch(const std::string& startID, const std::string& endID ,const std::string& mode = "driving") {
    // Step 1: 开始计时
    auto startTime = std::chrono::high_resolution_clock::now();

    if (mode != "driving" && mode != "walking" && mode != "cycling") {
        std::cout << "Invalid mode\n";
        return Json::Value();
    }

    if (startID.empty() || endID.empty()) {
        std::cout << "Start or End node not found!\n";
        return Json::Value(); // Return empty JSON on failure
    }

    const auto& adjacencyList =
        (mode == "driving") ? drivingAdjacencyList :
        (mode == "walking") ? walkingAdjacencyList : cyclingAdjacencyList;

    auto haversine = [&](double lat1, double lon1, double lat2, double lon2) {
        const double R = 6371.0; // Earth's radius in km
        double dLat = (lat2 - lat1) * M_PI / 180.0;
        double dLon = (lon2 - lon1) * M_PI / 180.0;
        lat1 = lat1 * M_PI / 180.0;
        lat2 = lat2 * M_PI / 180.0;

        double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
        double c = 2 * atan2(sqrt(a), sqrt(1 - a));
        return 0.85*R * c; // Distance in kilometers
    };

    double endLat = nodes[endID].first;
    double endLon = nodes[endID].second;

    auto heuristic = [&](const std::string& nodeID) {
        double nodeLat = nodes[nodeID].first;
        double nodeLon = nodes[nodeID].second;
        return haversine(nodeLat, nodeLon, endLat, endLon);
    };

    // Step 3: Priority queue for A* (min-heap)
    using State = std::pair<double, std::string>; // (f-score, nodeID)
    std::priority_queue<State, std::vector<State>, std::greater<>> openSet;

    // Step 4: Data structures for A*
    std::unordered_map<std::string, double> gScore;
    std::unordered_map<std::string, std::string> cameFrom;

    // Initialize
    gScore[startID] = 0.0;
    openSet.push({heuristic(startID), startID});

    // Step 5: A* Search Loop
    while (!openSet.empty()) {
        auto [currentF, currentID] = openSet.top();
        openSet.pop();

        // If we reached the destination
        if (currentID == endID) {
            // 记录结束时间
            auto endTime = std::chrono::high_resolution_clock::now();
            // 计算执行时间
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            std::cout << "A* search executed in " << duration.count() << " milliseconds." << std::endl;

            return reconstructPath(cameFrom, startID, endID);
        }

        // Expand neighbors
        auto it = adjacencyList.find(currentID);
        if (it != adjacencyList.end()) { // Check if the current node has neighbors
            for (const auto& entry : it->second) {
                auto neighborID = entry.first;
                auto edgeLength = entry.second;
                double tentativeGScore = gScore[currentID] + edgeLength;
                if (gScore.find(neighborID) == gScore.end() || tentativeGScore < gScore[neighborID]) {
                    gScore[neighborID] = tentativeGScore;
                    cameFrom[neighborID] = currentID;
                    double fScore = tentativeGScore + heuristic(neighborID);
                    openSet.push({fScore, neighborID});
                }
            }
        }
    }

    // If no path is found
    std::cout << "No path found from " << startID << " to " << endID << "!\n";
    return Json::Value(); // Empty JSON
}

Json::Value osmMap::Dijkstra(const std::string& startID, const std::string& endID, const std::string& mode) {
    // Step 1: 开始计时
    auto startTime = std::chrono::high_resolution_clock::now();

    if (mode != "driving" && mode != "walking" && mode != "cycling") {
        std::cout << "Invalid mode\n";
        return Json::Value();
    }

    if (startID.empty() || endID.empty()) {
        std::cout << "Start or End node not found!\n";
        return Json::Value(); // Return empty JSON on failure
    }

    const auto& adjacencyList =
        (mode == "driving") ? drivingAdjacencyList :
        (mode == "walking") ? walkingAdjacencyList : cyclingAdjacencyList;

    // Step 3: Priority queue for Dijkstra (min-heap)
    using State = std::pair<double, std::string>; // (distance, nodeID)
    std::priority_queue<State, std::vector<State>, std::greater<>> openSet;

    // Step 4: Data structures for Dijkstra
    std::unordered_map<std::string, double> dist;
    std::unordered_map<std::string, std::string> cameFrom;

    // Initialize
    dist[startID] = 0.0;
    openSet.push({0.0, startID});

    // Step 5: Dijkstra's Search Loop
    while (!openSet.empty()) {
        auto [currentDist, currentID] = openSet.top();
        openSet.pop();

        // If we reached the destination
        if (currentID == endID) {
            // 记录结束时间
            auto endTime = std::chrono::high_resolution_clock::now();
            // 计算执行时间
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            std::cout << "Dijkstra's search executed in " << duration.count() << " milliseconds." << std::endl;

            return reconstructPath(cameFrom, startID, endID);
        }

        // Expand neighbors
        auto it = adjacencyList.find(currentID);
        if (it != adjacencyList.end()) { // Check if the current node has neighbors
            for (const auto& entry : it->second) {
                auto neighborID = entry.first;
                auto edgeLength = entry.second;
                double tentativeDist = dist[currentID] + edgeLength;
                if (dist.find(neighborID) == dist.end() || tentativeDist < dist[neighborID]) {
                    dist[neighborID] = tentativeDist;
                    cameFrom[neighborID] = currentID;
                    openSet.push({tentativeDist, neighborID});
                }
            }
        }
    }

    // If no path is found
    std::cout << "No path found from " << startID << " to " << endID << "!\n";
    return Json::Value(); // Empty JSON
}




Json::Value osmMap::reconstructPath(const std::unordered_map<std::string, std::string>& cameFrom, 
                                    const std::string& startID, const std::string& endID) {
    Json::Value path(Json::arrayValue);
    
    std::string currentID = endID;

    // 反向重建路径并添加坐标
    while (currentID != startID) {
        double lat = nodes[currentID].first;
        double lon = nodes[currentID].second;
        Json::Value coordinate(Json::arrayValue);
        coordinate.append(lat); // 添加纬度
        coordinate.append(lon); // 添加经度
        path.append(coordinate); // 将坐标添加到路径
        if (cameFrom.find(currentID) == cameFrom.end()) break; // 防止进入无限循环
        currentID = cameFrom.at(currentID);
    }

    // 添加起点坐标
    double lat = nodes[startID].first;
    double lon = nodes[startID].second;
    Json::Value coordinate(Json::arrayValue);
    coordinate.append(lat); // 添加纬度
    coordinate.append(lon); // 添加经度
    path.append(coordinate); // 将起点坐标添加到路径

    // 反转路径，使其从起点到终点
    std::reverse(path.begin(), path.end());


    return path;
}




