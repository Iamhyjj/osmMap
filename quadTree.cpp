#include "quadTree.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <limits>
#include <iostream>

double QuadTree::distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0; // Earth's radius in km
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = sin(dLat / 2) * sin(dLat / 2) +
                cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c; // Distance in kilometers
}

void QuadTree::insert(QuadTreeNode*& node, Node* newNode) {

    // 确保新节点在当前区域内
    if (newNode->lat >= node->latMin && newNode->lat <= node->latMax &&
        newNode->lon >= node->lonMin && newNode->lon <= node->lonMax) {
            if(node->point!=nullptr){
                
                if(node->point->lat == newNode->lat && node->point->lon == newNode->lon){
                    return;
                } 
                double latMid = (node->latMax+node->latMin)/2;
                double lonMid = (node->lonMax+node->lonMin)/2;
                node->NW = new QuadTreeNode(latMid,node->lonMin,node->latMax,lonMid);
                node->NE = new QuadTreeNode(latMid,lonMid,node->latMax,node->lonMax);
                node->SW = new QuadTreeNode(node->latMin,node->lonMin,latMid,lonMid);
                node->SE = new QuadTreeNode(node->latMin,lonMid,latMid,node->lonMax);
                insertIntoSubtree(node,node->point);
                insertIntoSubtree(node,newNode);
                node->point = nullptr;            
            }
            else if(node->NE==nullptr){
                //std::cout << "Node " << newNode->ID << " has been inserted\n";
                node->point = newNode;
            }
            else{
                insertIntoSubtree(node,newNode);
            }
    }
}

void QuadTree::print(){
    print(root);
}

void QuadTree::print(QuadTreeNode* root){
    if(root==nullptr) return;
    if(root->point!=nullptr) std::cout << root->point->ID << '\n';
    print(root->NE);
    print(root->NW);
    print(root->SE);
    print(root->SW);
}

void QuadTree::insertIntoSubtree(QuadTreeNode* node, Node* newNode){
    double lat = newNode -> lat , lon = newNode -> lon;
    if (lat >= (node->latMin + node->latMax) / 2) {
        if (lon >= (node->lonMin + node->lonMax) / 2) {
            insert(node->NE, newNode); // 东北
        } else {
            insert(node->NW, newNode); // 西北
        }
    } else {
        if (lon >= (node->lonMin + node->lonMax) / 2) {
            insert(node->SE, newNode); // 东南
        } else {
            insert(node->SW, newNode); // 西南
        }
    }
}


Node* QuadTree::queryNearest(QuadTreeNode* node, double lat, double lon) {
    //leaf
    if(node->point!=nullptr){
        std::cout << "NearestNodeID : " << node -> point -> ID << '\n';
        return node->point;
    }
    
    Node* nearest = nullptr;
    bool notfound = 0;
    while(nearest == nullptr){
        // 如果是非叶节点，继续递归到对应的子树
        if (lat >= (node->latMin + node->latMax) / 2 || notfound) {
            if (lon >= (node->lonMin + node->lonMax) / 2 || notfound) {
                if( node -> NE && (node->NE->NE != nullptr || node->NE->point!=nullptr)){  //儿子为非空叶子节点或者非叶节点
                    nearest = queryNearest(node->NE, lat, lon); // 东北
                    notfound = 0;
                }
            } 
            if (lon < (node->lonMin + node->lonMax) / 2 || notfound) {
                if(node -> NW && (node->NW->NW != nullptr || node->NW->point!=nullptr)){
                    nearest = queryNearest(node->NW, lat, lon); // 西北
                    notfound = 0;
                }
            }
        }
        if (lat < (node->latMin + node->latMax) / 2 || notfound){
            if (lon >= (node->lonMin + node->lonMax) / 2 || notfound) {
                if(node -> SE && (node->SE->SE != nullptr || node->SE->point!=nullptr)){
                    nearest = queryNearest(node->SE, lat, lon); // 东南
                    notfound = 0;
                }
            } 
            if (lon < (node->lonMin + node->lonMax) / 2 || notfound) {
                if(node -> SW && (node->SW->SW != nullptr || node->SW->point!=nullptr)){
                    nearest = queryNearest(node->SW, lat, lon); // 西南
                    notfound = 0;
                }
            }
        }
        notfound = 1;
    }
    return nearest; 
}

QuadTree::QuadTree(double latMin, double lonMin, double latMax, double lonMax) {
    root = new QuadTreeNode(latMin, lonMin, latMax, lonMax);
}

void QuadTree::insert(Node* newNode) {
    insert(root, newNode);
}

Node* QuadTree::queryNearest(double lat, double lon) {

    return queryNearest(root, lat, lon);

}



