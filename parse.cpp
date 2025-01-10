#include <iostream>
#include<fstream>
#include <string>
#include<unordered_set>
#include <cmath>
#include<chrono>
#include <queue>
#include <functional>
#include <vector>
#include <limits>
#include <utility>
#include "fib.h"
#include "tinyxml/tinyxml.h"
#include "jsoncpp/json/json.h"
#include "quadTree.h"

double haversine(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0; // Earth's radius in km
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = sin(dLat / 2) * sin(dLat / 2) +
                cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c; // Distance in kilometers
};



class osmMap {
public:
    osmMap(const std::string& mapFilePath);
    void load(const std::string& mapFilePath);
    Json::Value findShortestPath(const std::string& startID, const std::string& endID ,const std::string& mode);
    void exportToJson(const std::string& outputFilePath);
    void loadFromJson(const std::string& inputFilePath);
private:
    void parseNodes(TiXmlElement* root);
    void parseWays(TiXmlElement* root);
    //void parseRelations(TiXmlElement* root);
    std::string findNearestNodeID(double startLat,double startLon);
    
    Json::Value reconstructPath(const std::unordered_map<std::string, std::string>& cameFrom, const std::string& startID, const std::string& endID);

    std::unordered_map<std::string,std::pair<double,double>> nodes;

    Json::Value ways;
    
    std::unordered_map<std::string, std::unordered_map<std::string, double>> drivingAdjacencyList;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> walkingAdjacencyList;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> cyclingAdjacencyList;

};



osmMap::osmMap(const std::string& mapFilePath) {
    load(mapFilePath);
}


void osmMap::exportToJson(const std::string& outputFilePath = "map_data.json") {
    Json::Value root;

    // 存储节点信息 (修改后的 nodes 数据结构)
    Json::Value nodesJson(Json::objectValue);
    for (const auto& [id, latLonPair] : nodes) {
        Json::Value nodeJson;
        nodeJson["lat"] = latLonPair.first;  // lat
        nodeJson["lon"] = latLonPair.second; // lon
        nodesJson[id] = nodeJson;
    }
    root["nodes"] = nodesJson;

    // 存储不同模式的邻接表
    auto exportAdjacencyList = [](const std::unordered_map<std::string, std::unordered_map<std::string, double>>& adjacencyList) {
        Json::Value adjacencyJson;
        for (const auto& [node, neighbors] : adjacencyList) {
            Json::Value neighborsJson(Json::arrayValue);
            for (const auto& [neighbor, distance] : neighbors) {
                Json::Value neighborJson(Json::objectValue);
                neighborJson["node"] = neighbor;   // 邻接节点
                neighborJson["distance"] = distance; // 边长（距离）
                neighborsJson.append(neighborJson);
            }
            adjacencyJson[node] = neighborsJson;
        }
        return adjacencyJson;
    };


    root["adjacencyLists"]["driving"] = exportAdjacencyList(drivingAdjacencyList);
    root["adjacencyLists"]["walking"] = exportAdjacencyList(walkingAdjacencyList);
    root["adjacencyLists"]["cycling"] = exportAdjacencyList(cyclingAdjacencyList);

    // 将数据写入文件
    std::ofstream outFile(outputFilePath, std::ios::out);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << outputFilePath << std::endl;
        return;
    }

    Json::StreamWriterBuilder writer;
    outFile << Json::writeString(writer, root);
    outFile.close();

    std::cout << "Nodes, ways, and adjacency lists exported to " << outputFilePath << std::endl;
}




void osmMap::load(const std::string& mapFilePath = "map") {
    //loadFromJson();
    TiXmlDocument tinyXmlDoc("map");
    tinyXmlDoc.LoadFile();
    TiXmlElement *root = tinyXmlDoc.RootElement();
    parseNodes(root);
    parseWays(root);
    std::cout << "Map file loaded successfully! Adjacency list built.\n";
}

void osmMap::parseNodes(TiXmlElement* root) {
    TiXmlElement* nodeElement = root->FirstChildElement("node");

    // 用于存储与道路相关的节点 ID
    std::unordered_set<std::string> roadNodes;

    // 遍历所有的 way 元素，查找带有 highway 标签的元素
    TiXmlElement* wayElement = root->FirstChildElement("way");
    for (; wayElement; wayElement = wayElement->NextSiblingElement("way")) {
        TiXmlElement* tagElement = wayElement->FirstChildElement("tag");
        
        // 检查是否有 highway 标签
        for (; tagElement; tagElement = tagElement->NextSiblingElement("tag")) {
            const char* key = tagElement->Attribute("k");
            const char* value = tagElement->Attribute("v");

            if (key && value && (std::string(key) == "highway" || std::string(key) == "route")) {
                // 如果是道路相关的 way，提取其中的节点
                TiXmlElement* ndElement = wayElement->FirstChildElement("nd");
                for (; ndElement; ndElement = ndElement->NextSiblingElement("nd")) {
                    const char* ref = ndElement->Attribute("ref");
                    if (ref) {
                        roadNodes.insert(ref);  // 将与道路相关的节点 ID 加入集合
                    }
                }
                break;  // 一旦找到与道路相关的标签，跳出循环
            }
        }
    }

    // 遍历所有 node 元素，只保留与道路相关的节点
    for (; nodeElement; nodeElement = nodeElement->NextSiblingElement("node")) {
        std::string nodeId = nodeElement->Attribute("id");
        double lat = std::stod(nodeElement->Attribute("lat"));
        double lon = std::stod(nodeElement->Attribute("lon"));

        // 只保留与道路相关的节点
        if (roadNodes.find(nodeId) != roadNodes.end()) {
            nodes[nodeId] = {lat,lon};
        }
    }

    std::cout << "Nodes parsed: " << nodes.size() << "\n";
}

void osmMap::parseWays(TiXmlElement* root) {

    static const std::unordered_set<std::string> drivingHighways = {
    "motorway", "trunk", "primary", "secondary", "tertiary",
    "unclassified", "residential", "motorway_link", "trunk_link",
    "secondary_link", "tertiary_link","primary_link","service","living_street"};
    static const std::unordered_set<std::string> walkingHighways = {
        "footway", "path", "pedestrian", "steps", "cycleway","residential"
        ,"living_street","secondary","corridor","track", "tertiary","service"};
    static const std::unordered_set<std::string> cyclingHighways = {
        "cycleway", "primary", "secondary", "tertiary", "unclassified", "residential"
        ,"track","primary_link","tertiary_link","secondary_link","service"};

    std::unordered_map<std::string, double> drivingWeightTable = {
        {"motorway", 0.5},
        {"trunk", 0.6},
        {"primary", 0.8},
        {"secondary", 1.0},
        {"tertiary", 1.2},
        {"unclassified", 1.5},
        {"residential", 1.8},
        {"motorway_link", 0.7},
        {"trunk_link", 0.8},
        {"primary_link", 0.9},
        {"secondary_link", 1.0},
        {"tertiary_link", 1.3},
        {"service",3.0},
        {"living_street",3.0}
    };


    std::unordered_map<std::string, double> walkingWeightTable = {
        {"footway", 0.5},        // 专用自行车道，权重较低
        {"pedestrian", 0.5},         // 主路，骑行较常见，但较为复杂
        {"path", 0.8},       // 次级路，骑行时较为常见，略复杂
        {"tertiary", 1.0},        // 第三级路，相对不常见，骑行较为艰难
        {"unclassified", 1.2},    // 无类别道路，不确定，可能较差
        {"residential", 1.5},     
        {"track", 0.8},           // 小道，通常较适合骑行，但可能不平坦
        {"primary_link", 0.6},    
        {"tertiary_link", 1.0},   
        {"secondary_link", 0.8},  
        {"service", 3.0}          
    };
    
    std::unordered_map<std::string, double> cyclingWeightTable = {
        {"cycleway", 0.4},        // 专用自行车道，权重较低
        {"primary", 0.5},         // 主路，骑行较常见，但较为复杂
        {"secondary", 0.8},       // 次级路，骑行时较为常见，略复杂
        {"tertiary", 1.0},        // 第三级路，相对不常见，骑行较为艰难
        {"unclassified", 1.2},    // 无类别道路，不确定，可能较差
        {"residential", 1.5},     
        {"track", 0.8},           // 小道，通常较适合骑行，但可能不平坦
        {"primary_link", 0.6},    
        {"tertiary_link", 1.0},   
        {"secondary_link", 0.8},  
        {"service", 3.0}          
    };


    TiXmlElement* wayElement = root->FirstChildElement("way");
    for (; wayElement; wayElement = wayElement->NextSiblingElement("way")) {
        
        bool isOneWay = false;
        bool isDrivingAllowed = false;
        bool isWalkingAllowed = false;
        
        bool isCyclingAllowed = false;
        bool specialDrivingRules = true;  //默认没有特殊规则限制通行
        bool specialCyclingRules = true;
        bool specialWalkingRules = true;
        
        
        double drivingWeight = 1.0;
        double cyclingWeight = 1.0;
        double walkingWeight = 1.0;


        TiXmlElement* tagElement = wayElement->FirstChildElement("tag");
        for (; tagElement; tagElement = tagElement->NextSiblingElement("tag")) {
            std::string key = tagElement->Attribute("k");
            std::string value = tagElement->Attribute("v");

            if (key == "oneway" && value == "yes") {
                    isOneWay = true;
            }

            if (key == "access" && value == "no") {
                    isDrivingAllowed = false;
                    isWalkingAllowed = false;
                    isCyclingAllowed = false;
                    break;
            }

            if(key == "bicycle"){
                if(value=="designated"){
                    isCyclingAllowed = true;
                    cyclingWeight = 0.5;
                }
                if(value == "no"){
                    specialCyclingRules = false;
                    isCyclingAllowed = false;
                }
                if(value=="yes") isCyclingAllowed =true;
            }

            if(key=="route"&&value=="ferry"){
                isWalkingAllowed = true;
                isCyclingAllowed =true;
            }

            if(key=="motor_vehicle"){
                if(value=="no"){
                    isDrivingAllowed = false;
                    specialDrivingRules = false;
                }
                if(value=="designated"){
                    isDrivingAllowed =true;
                }
            }

            if(key=="foot"){
                if(value=="yes") isWalkingAllowed = true;
                if(value=="no")  isWalkingAllowed = false , specialWalkingRules = false;
            }

            if(key == "footway"){
                if(value=="no"){
                    isWalkingAllowed = false;
                    specialWalkingRules = false;
                }
            }

            if(key == "highway"){

                if (specialDrivingRules && drivingHighways.find(value) != drivingHighways.end()) {
                    isDrivingAllowed = true;
                    drivingWeight = drivingWeightTable[value];
                }
                if (specialWalkingRules && walkingHighways.find(value) != walkingHighways.end()) {
                    isWalkingAllowed = true;
                }
                if (specialCyclingRules && cyclingHighways.find(value) != cyclingHighways.end()) {
                    isCyclingAllowed = true;
                }
            }
        }

        // Parse nodes in the way
        if(!isDrivingAllowed && !isWalkingAllowed && !isCyclingAllowed) continue;
        TiXmlElement* ndElement = wayElement->FirstChildElement("nd");
        std::string prevNode;
        for (; ndElement; ndElement = ndElement->NextSiblingElement("nd")) {
            std::string currentNode = ndElement->Attribute("ref");
            if (!prevNode.empty()) {
                try {
                    // 尝试访问并转换经纬度
                    double prevLat = nodes[prevNode].first;
                    double prevLon = nodes[prevNode].second;
                    double currLat = nodes[currentNode].first;
                    double currLon = nodes[currentNode].second;

                    // 计算两点之间的地理距离
                    double distance = haversine(prevLat, prevLon, currLat, currLon);

                    // 更新邻接表
                    if (isDrivingAllowed) {
                        if (isOneWay) {
                            drivingAdjacencyList[prevNode][currentNode] = distance * drivingWeight;
                        } else {
                            drivingAdjacencyList[prevNode][currentNode] = distance * drivingWeight;
                            drivingAdjacencyList[currentNode][prevNode] = distance * drivingWeight;
                        }
                    }

                    if (isWalkingAllowed) {
                        if (isOneWay) {
                            walkingAdjacencyList[prevNode][currentNode] = distance * walkingWeight;
                        } else {
                            walkingAdjacencyList[prevNode][currentNode] = distance * walkingWeight;
                            walkingAdjacencyList[currentNode][prevNode] = distance * walkingWeight;
                        }
                    }

                    if (isCyclingAllowed) {
                        if (isOneWay) {
                            cyclingAdjacencyList[prevNode][currentNode] = distance * cyclingWeight;
                        } else {
                            cyclingAdjacencyList[prevNode][currentNode] = distance * cyclingWeight;
                            cyclingAdjacencyList[currentNode][prevNode] = distance * cyclingWeight;
                        }
                    }

                } catch (const std::invalid_argument& e) {
                    std::cerr << "Error: Invalid argument when parsing node coordinates for nodes " 
                            << prevNode << " and " << currentNode << std::endl;
                    std::cerr << "Exception: " << e.what() << std::endl;
                } catch (const std::out_of_range& e) {
                    std::cerr << "Error: Node ID out of range when accessing nodes " 
                            << prevNode << " and " << currentNode << std::endl;
                    std::cerr << "Exception: " << e.what() << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "General error when processing nodes " 
                            << prevNode << " and " << currentNode << std::endl;
                    std::cerr << "Exception: " << e.what() << std::endl;
                }
            }

            prevNode = currentNode;
        }
    }
}




int main() {

    osmMap map("map");
    map.exportToJson();

    return 0;
}