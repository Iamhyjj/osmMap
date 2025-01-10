#pragma once
#include<string>
#include <cmath>
#include <limits>

struct Node {
    double lat, lon;
    std::string ID; 

    Node(double latitude, double longitude, std::string id)
        : lat(latitude), lon(longitude), ID(id) {}
};

class QuadTree {
private:
    struct QuadTreeNode {
        double latMin, lonMin, latMax, lonMax; // 区域边界
        Node* point; // 当前点
        QuadTreeNode* NE; // 东北子树
        QuadTreeNode* NW; // 西北子树
        QuadTreeNode* SE; // 东南子树
        QuadTreeNode* SW; // 西南子树

        QuadTreeNode(double latMin, double lonMin, double latMax, double lonMax)
            : latMin(latMin), lonMin(lonMin), latMax(latMax), lonMax(lonMax), point(nullptr),
              NE(nullptr), NW(nullptr), SE(nullptr), SW(nullptr) {}
    };

    QuadTreeNode* root;

    double distance(double lat1, double lon1, double lat2, double lon2);
    void insert(QuadTreeNode*& node, Node* newNode);
    Node* queryNearest(QuadTreeNode* node, double lat, double lon);
    void insertIntoSubtree(QuadTreeNode* node,Node* newNode);
    void print(QuadTreeNode* root);

public:
    QuadTree(){}
    QuadTree(double latMin, double lonMin, double latMax, double lonMax);
    void insert(Node* newNode);
    Node* queryNearest(double lat, double lon);
    void print();
};


