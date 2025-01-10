#include <iostream>
#include <string>
#include <memory> // for std::unique_ptr
#include "osmMap.h"
#include "tinyxml/tinyxml.h"
#include "jsoncpp/json/json.h"
#include <emscripten/bind.h>

// 全局的 osmMap 指针
std::unique_ptr<osmMap> mapInstance;

// 初始化接口
void InitializeMap() {
    if (mapInstance) {
        std::cerr << "osmMap instance already initialized." << std::endl;
        return;
    }
    mapInstance = std::make_unique<osmMap>();
    std::cout << "osmMap initialized\n";
}

// 检查是否已初始化
bool IsMapInitialized() {
    return mapInstance != nullptr;
}

std::string FetchNearestNode(double lat,double lon,std::string mode = "driving"){
    return mapInstance->fetchNearestNode(lat,lon,mode);
}

// 查找最短路径接口
std::string FindShortestWay(const std::string& startID, const std::string& endID, const std::string& mode,const std::string& algorithm) {
    if (!mapInstance) {
        throw std::runtime_error("osmMap instance is not initialized.");
    }

    Json::Value result = mapInstance->findShortestPath(startID, endID, mode,algorithm);
    Json::StreamWriterBuilder writer;
    return Json::writeString(writer, result); // 返回 JSON 字符串
}

EMSCRIPTEN_BINDINGS(osm_map_bindings) {
    emscripten::function("InitializeMap", &InitializeMap);      // 初始化接口
    emscripten::function("IsMapInitialized", &IsMapInitialized); // 检查是否已初始化
    emscripten::function("FindShortestWay", &FindShortestWay);  // 查找最短路径接口
    emscripten::function("FetchNearestNode", &FetchNearestNode);  // 查找最短路径接口
}
