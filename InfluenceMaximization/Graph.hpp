//
//  Graph.hpp
//  InfluenceMaximization
//
//  Created by Madhavan R.P on 8/4/17.
//  Copyright © 2017 Madhavan R.P. All rights reserved.
//

#ifndef Graph_hpp
#define Graph_hpp

#include <stdio.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdlib.h>
#include <queue>
#include <ctime>
#include <deque>
#include <string.h>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
using namespace std;

class Graph {
private:
    float propogationProbability;
    int propogationProbabilityNumber;
    bool standardProbability;
    string graphName;
    float percentageTargets;
    vector<int> nonTargets;
    int numberOfTargets;
    int numberOfNonTargets;

public:
    Graph();
    int n, m;
    vector<vector<int> > graph;
    vector<vector<int> > graphTranspose;
    vector<vector<int>> rrSets;
    vector<bool> labels;
    deque<int> q;
    vector<int> inDegree;
    vector<bool> visited;
    vector<int> visitMark;
    vector<int> NodeinRRsetsWithCounts;
    vector<vector<set<int>>> associatedSet;
    vector<vector<int>>nodeAS;
    vector<unordered_map<int,unordered_set<int>>> pairAssociatedSet;
    vector<int> coverage;
    void readGraph(string fileName);
    void readGraph(string fileName, float percentage);
    void readInfluencedGraph(string fileName, float percentage,vector<int> activatedSet);
    void readHalfGraph(string fileName, float percentage);
    void readLabels(string fileName);
    void writeLabels();
    void setLabels(vector<bool> labels, float percentageTargets);
    
    //Numbers
    int getNumberOfVertices();
    int getNumberOfEdges();
    int getNumberOfTargets();
    int getNumberOfNonTargets();
    
    //Data Structure
    vector<int> *getNonTargets();
    
    vector<vector<int>> constructTranspose(vector<vector<int> > aGraph);
    void generateRandomRRSets(int R, bool label);
    void generateRandomRRSetsFromTargets(int R, vector<int> activatedSet);
    
    vector<int> generateRandomRRSet(int randomVertex, int rrSetID);
    void generateRandomRRSetwithCount(int randomVertex, int rrSetID);
    void clearRandomRRSets();
    vector<vector<int>>* getRandomRRSets();
    
    vector<int> oldRRSetGeneration(int randomVertex, int rrSetID);
    void assertTransposeIsCorrect();
    
    //Functions for propogation probability
    void setPropogationProbability(float p);
    bool flipCoinOnEdge(int u, int v);
    int generateRandomNumber(int u, int v);
    int getPropogationProbabilityNumber();
    void removeOutgoingEdges(int v);
    void removeNodeFromRRset(int v);
};

#endif /* Graph_hpp */



