//
//  main.cpp
//  InfluenceMaximization
//
//  Created by Madhavan R.P on 8/4/17.
//  Copyright © 2017 Madhavan R.P. All rights reserved.
//

#include <iostream>
#include "cxxopts.hpp"
#include "InfluenceMaximization/Graph.hpp"
#include "InfluenceMaximization/IMTree.hpp"
#include "InfluenceMaximization/EstimateNonTargets.hpp"
#include "InfluenceMaximization/TIMUtility.hpp"
#include "InfluenceMaximization/Phase2.hpp"
#include "InfluenceMaximization/SeedSet.hpp"
#include "InfluenceMaximization/Diffusion.hpp"
#include "InfluenceMaximization/IMResults/IMResults.h"
#include "InfluenceMaximization/memoryusage.h"
#include <string>
#include <chrono>
#include "InfluenceMaximization/log.h"
#include "InfluenceMaximization/DifferenceApproximator.hpp"
#include "InfluenceMaximization/GenerateGraphLabels.hpp"
#include "InfluenceMaximization/BaselineGreedy.hpp"
#include "InfluenceMaximization/BaselineGreedyTIM.hpp"

#include <iomanip>
#include <ctime>
#include <sstream>

using json = nlohmann::json;
using namespace std;

#define PHASE1TIM_PHASE2TIM 1;
#define PHASE1SIM_PHASE2SIM 2;

void setupLogger() {
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(buffer,sizeof(buffer),"%d-%m-%Y-%I:%M:%S",timeinfo);
    std::string str(buffer);
    FILELog::ReportingLevel() = logDEBUG3;
    string logFileName = "logs/influence-" + str + ".log";
    FILE* log_fd = fopen( logFileName.c_str(), "w" );
    Output2FILE::Stream() = log_fd;
}

bool sortbysecdesc(const pair<int,int> &a,const pair<int,int> &b)
{
    return (a.second > b.second);
}

bool sortbydegree(const int &a,const int &b)
{
    return (a > b);
}

void testApprox(Graph *graph, int budget, ApproximationSetting setting, bool extendPermutation) {
    DifferenceApproximator differenceApproximator(graph);
    differenceApproximator.setN(graph->n);
    set<int> seedSet;
    vector<int> permutation = differenceApproximator.generatePermutation();
    ModularApproximation modularApprox(permutation, setting);
    modularApprox.createTIMEvaluator(graph);
    modularApprox.findAllApproximations();
    if(setting==setting3) {
        if(!extendPermutation) {
            seedSet = differenceApproximator.executeAlgorithmApproximatingOneFunction(setting, budget);
        } else {
            seedSet = differenceApproximator.executeAlgorithmApproximatingOneFunctionExtendPermutation(setting, budget);
        }
    } else {
        if(!extendPermutation) {
            seedSet = differenceApproximator.executeGreedyAlgorithm(graph, &modularApprox, budget);
        } else {
            seedSet = differenceApproximator.executeGreedyAlgorithmAdjustingPermutation(setting, budget);
        }
    }
    pair<int, int> influence = findInfluenceUsingDiffusion(graph, seedSet, NULL);
    cout <<"\n Results after Diffusion: ";
    cout << "\nInfluence Targets: " << influence.first;
    cout << "\nInfluence NT: " << influence.second;
    IMResults::getInstance().setApproximationInfluence(influence);
    
}

void loadResultsFileFrom(cxxopts::ParseResult result) {
    // Necessary paramters
    int budget = result["budget"].as<int>();
    string graphFileName = result["graph"].as<std::string>();
    int percentageTargets = result["percentage"].as<int>();
    float percentageTargetsFloat = (float)percentageTargets/(float)100;
    string algorithm = result["algorithm"].as<string>();
    IMResults::getInstance().setBudget(budget);
    IMResults::getInstance().setGraphName(graphFileName);
    IMResults::getInstance().setPercentageTargets(percentageTargets);
    IMResults::getInstance().setAlgorithm(algorithm);
    
    // Optional parameters
    if(result["threshold"].count()>0) {
        int nonTargetThreshold = result["threshold"].as<int>();
        IMResults::getInstance().setNonTargetThreshold(nonTargetThreshold);
    }
    IMResults::getInstance().setPropagationProbability("inDegree");
    if(result.count("p")>0) {
        double probability = result["p"].as<double>();
        IMResults::getInstance().setPropagationProbability(probability);
    }
    
    if(result.count("ntfile")>0) {
        string nonTargetsFileName = result["ntfile"].as<std::string>();
        IMResults::getInstance().setFromFile(true);
        IMResults::getInstance().setNonTargetFileName(nonTargetsFileName);
    }
}

void loadGraphSizeToResults(Graph *graph) {
    IMResults::getInstance().setNumberOfVertices(graph->getNumberOfVertices());
    IMResults::getInstance().setNumberOfEdges(graph->getNumberOfEdges());
}

set<int> removeVertices(Graph *influencedGraph,Graph *graph, int removeNodes, set<int> seedSet, vector<int> activatedSet, string modular,set<int> alreadyinSeed){
    //Random RR sets
    int n = (int)activatedSet.size();
    double epsilon = (double)EPSILON;
    int R = (8+2 * epsilon) * n * (2 * log(n) + log(2))/(epsilon * epsilon);
    influencedGraph->generateRandomRRSetsFromTargets(R, activatedSet, modular);
    cout << "\n RRsets done " << flush;
    
    //clearing the memory
    vector<int>().swap(activatedSet);
    vector<vector<int>>().swap(influencedGraph->rrSets);
    
    //Find nodes to be removed
    vector<pair<int,int>> SortedNodeidCounts=vector<pair<int,int>>();
    for(int i=0;i<influencedGraph->NodeinRRsetsWithCounts.size();i++){
        pair<int,int> node= pair<int,int>();
        node.first=i;
        node.second=influencedGraph->NodeinRRsetsWithCounts[i];
        SortedNodeidCounts.push_back(node);
    }
    vector<int>().swap(influencedGraph->NodeinRRsetsWithCounts);
    
    std :: sort(SortedNodeidCounts.begin(),SortedNodeidCounts.end(), sortbysecdesc);
    assert(SortedNodeidCounts.at(0).second>SortedNodeidCounts.at(1).second);
    
    set<int> nodesToRemove=set<int>();
    int i=0;
    int j=0;
    while(j<removeNodes && j< SortedNodeidCounts.size()){
        int nodeid=SortedNodeidCounts.at(i).first;
        if(nodesToRemove.count(nodeid)==0 && seedSet.count(nodeid)==0){
            nodesToRemove.insert(nodeid);
            j++;
            /*if(seedSet.count(nodeid)==1){
             alreadyinSeed.insert(nodeid);
             }*/
        }
        i++;
    }
    vector<pair<int,int>>().swap(SortedNodeidCounts);
    cout << "\n Number of nodes Already present in seed set = " << alreadyinSeed.size();
    return  nodesToRemove;
}

void checkMod(string graphFileName, float percentageTargetsFloat, Graph* graph,set<int> seedSet,int budget,bool useIndegree, float probability){
    vector<int> activatedSet=performDiffusion(graph,seedSet,NULL);
    cout << "\n submod activated size" <<activatedSet.size();
    
    set<int> modseedSet;
    Graph *modGraph = new Graph;
    modGraph->readGraph(graphFileName, percentageTargetsFloat);
    if(!useIndegree) {
        modGraph->setPropogationProbability(probability);
    }
    int n = (int)modGraph->getNumberOfVertices();
    double epsilon = (double)EPSILON;
    int R = (8+2 * epsilon) * n * (2 * log(n) + log(2))/(epsilon * epsilon);
    modGraph->generateRandomRRSets(R, false);
    vector<int> NodeinRRsets=vector<int>() ;
    NodeinRRsets=modGraph->NodeinRRsetsWithCounts;
    vector<int>().swap(modGraph->NodeinRRsetsWithCounts);
    
    vector<pair<int,int>> SortedNodeidCounts=vector<pair<int,int>>();
    for(int i=0;i<NodeinRRsets.size();i++){
        pair<int,int> node= pair<int,int>();
        node.first=i;
        node.second=NodeinRRsets[i];
        SortedNodeidCounts.push_back(node);
    }
    
    vector<int>().swap(NodeinRRsets);
    
    std :: sort(SortedNodeidCounts.begin(),SortedNodeidCounts.end(), sortbysecdesc);
    int j=0;
    for(int i=0;i<budget;i++){
        modseedSet.insert(SortedNodeidCounts[i].first);
        if(seedSet.count(SortedNodeidCounts[i].first)==1)
            j++;
    }
    cout<<"\n intersection of submodular and modular "<< j;
    cout<<"\n Selected k mod SeedSet: " << flush;
    for(auto item:modseedSet)
        cout<< item << " ";
    vector<int> modActivatedSet=performDiffusion(modGraph,modseedSet,NULL);
    cout << "\n mod activated size" <<modActivatedSet.size();
    
    //intersection of influenced graph of mod and submod seed sets
    std::sort(modActivatedSet.begin(), modActivatedSet.end());
    std::sort(activatedSet.begin(), activatedSet.end());
    std::vector<int> v_intersection;
    std::set_intersection(modActivatedSet.begin(), modActivatedSet.end(),activatedSet.begin(), activatedSet.end(),std::back_inserter(v_intersection));
    cout << "\n influence intersection of mod and submod" <<v_intersection.size();
}

int removeVerticesIterative(Graph *influencedGraph, vector<int> activatedSet,string modular){
    vector<pair<int,int>> SortedNodeidCounts=vector<pair<int,int>>();
    
    for(int v=0;v<influencedGraph->getNumberOfVertices();v++){
        //create graph removing one node at a time
        Graph *newGraph = new Graph(*influencedGraph);
        newGraph->removeOutgoingEdges(v);
        
        //Random RR sets
        int n = (int)activatedSet.size();
        double epsilon = (double)EPSILON;
        int R = (8+2 * epsilon) * n * (2 * log(n) + log(2))/(epsilon * epsilon);
        newGraph->generateRandomRRSetsFromTargets(R, activatedSet,modular);
        //cout << "\n RRsets done for node" << v<< flush;
        
        //clearing the memory
        vector<vector<int>>().swap(newGraph->rrSets);
        
        //summation of influence after removing node v
        pair<int,int> node= pair<int,int>();
        node.first=v;
        for(int i: newGraph->NodeinRRsetsWithCounts){
            node.second+=i;
        }
        SortedNodeidCounts.push_back(node);
        vector<int>().swap(newGraph->NodeinRRsetsWithCounts);
        delete newGraph;
    }
    
    //Find nodes to be removed
    std :: sort(SortedNodeidCounts.begin(),SortedNodeidCounts.end(), sortbysecdesc);
    assert(SortedNodeidCounts.at(0).second>=SortedNodeidCounts.at(1).second);
    return SortedNodeidCounts.at(SortedNodeidCounts.size()-1).first;
}



set<int> runTim(Graph *graph,bool fromFile,string nonTargetsFileName,int method,int budget,int nonTargetThreshold,string graphFileName,int percentageTargets){
    loadGraphSizeToResults(graph);
    vector<double> nodeCounts;
    clock_t phase1StartTime = clock();
    cout << "\n Before any estimate non targets creation:";
    disp_mem_usage("");
    EstimateNonTargets *estimateNonTargets = NULL;
    if(!fromFile) {
        estimateNonTargets = new EstimateNonTargets(graph);
        if(method==1) {
            nodeCounts = estimateNonTargets->getNonTargetsUsingTIM();
        } else {
            nodeCounts = estimateNonTargets->getNonTargetsUsingSIM();
        }
    } else {
        estimateNonTargets = new EstimateNonTargets();
        estimateNonTargets->readFromFile(nonTargetsFileName);
        nodeCounts = *estimateNonTargets->getAllNonTargetsCount();
        delete estimateNonTargets;
    }
    cout << "\n Non targets file is alive ";
    disp_mem_usage("");
    clock_t phase1EndTime = clock();
    
    FILE_LOG(logDEBUG) << "Completed Phase 1";
    double phase1TimeTaken = double(phase1EndTime - phase1StartTime) / CLOCKS_PER_SEC;
    IMResults::getInstance().setPhase1Time(phase1TimeTaken);
    if(!fromFile) {
        nonTargetsFileName = graphFileName;
        nonTargetsFileName+="_" + to_string(budget);
        nonTargetsFileName+="_" + to_string(nonTargetThreshold);
        nonTargetsFileName+="_" + to_string(percentageTargets);
        nonTargetsFileName+="_" + to_string(rand() % 1000000);
        nonTargetsFileName+="_1";
        nonTargetsFileName+=".txt";
        estimateNonTargets->writeToFile(nonTargetsFileName);
        cout << "\nWriting Non Targets to file " << nonTargetsFileName;
        cout << "\n";
        IMResults::getInstance().setNonTargetFileName(nonTargetsFileName);
        delete estimateNonTargets;
    }
    cout << "\n Non Target file is dead ";
    disp_mem_usage("");
    cout << "\n Should be same as before" << flush;
    //Start phase 2
    cout <<"Starting phase 2";
    FILE_LOG(logDEBUG) << "Starting phase 2";
    clock_t phase2StartTime = clock();
    Phase2 *phase2= NULL;
    if(method==1) {
        phase2 = new Phase2TIM(graph);
    }
    else {
        phase2 = new Phase2SIM(graph);
    }
    phase2->doPhase2(budget, nonTargetThreshold, nodeCounts);
    IMResults::getInstance().addBestSeedSet(phase2->getTree()->getBestSeedSet(budget));
    clock_t phase2EndTime = clock();
    double phase2TimeTaken = double(phase2EndTime - phase2StartTime) / CLOCKS_PER_SEC;
    FILE_LOG(logDEBUG) << "Completed phase 2 ";
    
    IMResults::getInstance().setPhase2Time(phase2TimeTaken);
    IMResults::getInstance().setTotalTimeTaken(phase1TimeTaken + phase2TimeTaken);
    
    vector<IMSeedSet> allSeedSets = phase2->getTree()->getAllSeeds(budget);
    IMResults::getInstance().addSeedSets(allSeedSets);
    IMSeedSet bestSeedSet = phase2->getTree()->getBestSeedSet(budget);
    delete phase2;
    return bestSeedSet.getSeedSet();
}


int modifiedremoveVerticesIterative(Graph *influencedGraph, vector<int> activatedSet, set<int>nodesToremove, string modular){
    //Random RR sets
    int n = (int)activatedSet.size();
    double epsilon = (double)EPSILON;
    int R = (8+2 * epsilon) * n * (2 * log(n) + log(2))/(epsilon * epsilon);
    cout<< "RR sets are: "<<R;
    influencedGraph->generateRandomRRSetsFromTargets(R, activatedSet,modular);
    
    //clearing the memory
    vector<int>().swap(activatedSet);
    vector<int>().swap(influencedGraph->NodeinRRsetsWithCounts);
    
    vector<pair<int,int>> SortedNodeidCounts=vector<pair<int,int>>();
    //Find nodes to be removed
    for(int i=0;i<influencedGraph->getNumberOfVertices();i++){
        pair<int,int> node= pair<int,int>();
        node.first=i;
        for(int j=0;j<influencedGraph->getNumberOfVertices();j++){
            for(int r=0;r<R;r++){
                //if(influencedGraph->newrrSets[r].count(j)==1){
                /*if(influencedGraph->associatedSet[r].at(j).count(i)==1 ){
                 node.second++;
                 continue;
                 }
                 for(int v:nodesToremove){
                 if(influencedGraph->associatedSet[r].at(j).count(v)==1)*/
                node.second++;
                //}
                //}
            }
        }
        SortedNodeidCounts.push_back(node);
    }
    
    std :: sort(SortedNodeidCounts.begin(),SortedNodeidCounts.end(), sortbysecdesc);
    assert(SortedNodeidCounts.at(0).second>=SortedNodeidCounts.at(1).second);
    
    return SortedNodeidCounts.at(0).first;
}

int getMarginalLoss(Graph *influencedGraph, vector<int> activatedSet,set<int>nodesToremove)
{
    int vertexnum=influencedGraph->getNumberOfVertices();
    //influencedGraph->coverage=vector<int>(vertexnum);
    vector<int> checkCoverage=vector<int>(vertexnum,0);
    int maxsum=0;
    int maxIndex=-1;
    for(int i=0;i<vertexnum;i++){
        int total=0;
        for(pair<int,unordered_set<int>> j : influencedGraph->pairAssociatedSet[i]){
            //total+=influencedGraph->associatedSet[i][j].size();
            total+=j.second.size();
        }
        //influencedGraph->coverage[i]=total;
        checkCoverage[i]=total;
        if(checkCoverage[i]!=influencedGraph->coverage[i]){
            cout<<"actual "<<checkCoverage[i]<<"calculated "<<influencedGraph->coverage[i];
        }
        if(total>maxsum){
            maxIndex=i;
            maxsum=total;
        }
    }
    
    return maxIndex;
}

set<int> getSeed(Graph *graph,int budget,vector<int> activatedSet){
    vector<vector<int>> lookupTable;
    TIMCoverage timCoverage(&lookupTable);
    double epsilon = 2;
    int n = graph->n;
    int R = (8+2 * epsilon) * n * (2 * log(n) + log(2))/(epsilon * epsilon);
    graph->generateRandomRRSets(R, true);
    vector<vector<int>> *randomRRSets = graph->getRandomRRSets();
    timCoverage.initializeLookupTable(randomRRSets, graph->n);
    timCoverage.initializeDataStructures((int)randomRRSets->size(), graph->n);
    unordered_set<int> activatedNodes;
    activatedNodes.insert(activatedSet.begin(),activatedSet.end());
    set<int> seedSet = timCoverage.findTopKNodes(budget, randomRRSets,activatedNodes);
    return seedSet;
}

void newDiffusion(Graph *newGraph,Graph *subNewGraph, set<int>modNodes,set<int>subModNodes,vector<int> activatedSet,int budget,int topBestThreshold){
    cout<< "\n nodes To remove in mod graph ";
    for(int i:modNodes){
        cout<< i << " ";
        newGraph->removeOutgoingEdges(i);
        assert(newGraph->graph[i].size()==0);
        assert(newGraph->graphTranspose[i].size()==0);
    }
    
    //int k=0;
    //while(k<=10){
    set<int> seedSet=set<int>();
    SeedSet *SeedClass=new SeedSet(newGraph , budget);
    seedSet=SeedClass->outdegreeRandom(topBestThreshold,modNodes,subModNodes);
    //seedSet=getSeed(newGraph, budget,activatedSet);
    //seedSet=runTim(newGraph,fromFile,nonTargetsFileName,method,budget,nonTargetThreshold, graphFileName, percentageTargets);
    cout<<"\n Selected new SeedSet: " << flush;
    for(auto item:seedSet)
        cout<< item << " ";
    
    //again diffusion on old graph after node removal
    vector<int> NewactivatedSet=performDiffusion(newGraph,seedSet,NULL);
    
    //find intersection of new and old activated set
    std::vector<int> intersect;
    std::sort(NewactivatedSet.begin(), NewactivatedSet.end());
    std::sort(activatedSet.begin(), activatedSet.end());
    std::set_intersection(activatedSet.begin(), activatedSet.end(),NewactivatedSet.begin(), NewactivatedSet.end(),std::back_inserter(intersect));
    
    cout << "\n Old Targets activated = " << activatedSet.size();
    cout << "\n New Targets activated = " << NewactivatedSet.size();
    cout << "\n intersection size "<<intersect.size();
    cout << "\n Percentage of intersect with old " <<double((intersect.size()*100)/activatedSet.size())<<"%";
    
    cout<< "\n nodes To remove in submod graph "<<flush;
    for(int i:subModNodes){
        cout<< i << " ";
        subNewGraph->removeOutgoingEdges(i);
        assert(subNewGraph->graph[i].size()==0);
        assert(subNewGraph->graphTranspose[i].size()==0);
    }
    //again diffusion on old graph after node removal
    vector<int> SubNewactivatedSet=performDiffusion(subNewGraph,seedSet,NULL);
    
    //find intersection of new and old activated set
    std::vector<int> subIntersect;
    std::sort(SubNewactivatedSet.begin(), SubNewactivatedSet.end());
    std::sort(activatedSet.begin(), activatedSet.end());
    std::set_intersection(activatedSet.begin(), activatedSet.end(),SubNewactivatedSet.begin(), SubNewactivatedSet.end(),std::back_inserter(subIntersect));
    
    cout << "\n Old Targets activated = " << activatedSet.size();
    cout << "\n New Targets activated = " << SubNewactivatedSet.size();
    cout << "\n intersection size "<<subIntersect.size();
    cout << "\n Percentage of intersect with old " <<double((subIntersect.size()*100)/activatedSet.size())<<"%";
    
    // k++;
    //}
}

void executeTIMTIM(cxxopts::ParseResult result) {
    clock_t executionTimeBegin = clock();
    cout << "\n begin execution tim tim ";
    int budget;
    int nonTargetThreshold;
    string graphFileName;
    int percentageTargets;
    bool fromFile = false;
    string nonTargetsFileName;
    int method = PHASE1TIM_PHASE2TIM;
    bool useIndegree = true;
    float probability = 0;
    int removeNodes=0;
    string seedSelection;
    string modular;
    int topBestThreshold=100;
    
    budget = result["budget"].as<int>();
    nonTargetThreshold = result["threshold"].as<int>();
    graphFileName = result["graph"].as<std::string>();
    percentageTargets = result["percentage"].as<int>();
    removeNodes=result["nodesRemove"].as<int>();
    seedSelection=result["seedset"].as<std::string>();
    modular=result["modularity"].as<std::string>();
    
    loadResultsFileFrom(result);
    
    if(result.count("method")>0) {
        method = result["method"].as<int>();
    }
    if(result.count("p")>0) {
        probability = result["p"].as<double>();
        useIndegree = false;
    }
    if(result.count("ntfile")>0) {
        nonTargetsFileName = result["ntfile"].as<std::string>();
        fromFile = true;
    }
    
    // Log information
    cout << "\n Conducting experiments for:\n";
    cout <<" Graph: " << graphFileName;
    cout << "\t Budget: " << budget;
    cout << "\t Non Target Threshod: " << nonTargetThreshold;
    cout << "\t Percentage:  " << percentageTargets;
    cout << "\t Method: " << method;
    cout << "\t Nodes removed: " << removeNodes;
    cout << "\t Seed selection : " << seedSelection;
    cout << "\t Top best outdegree threshold : " <<topBestThreshold;
    
    if(useIndegree) {
        cout << "\t Probability: Indegree";
    } else {
        cout << "\t Probability: " <<  probability;
    }
    if(fromFile) {
        cout << "\n Reading Non targets from file: " << nonTargetsFileName;
    }
    
    FILE_LOG(logDEBUG) << "\n Conducting experiments for:\n";
    FILE_LOG(logDEBUG) <<" Graph: " << graphFileName;
    FILE_LOG(logDEBUG) << "\t Budget: " << budget;
    FILE_LOG(logDEBUG) << "\t Non Target Threshod: " << nonTargetThreshold;
    FILE_LOG(logDEBUG) << "\t Percentage:  " << percentageTargets;
    FILE_LOG(logDEBUG) << "\t Method: " << method;
    if(fromFile) {
        FILE_LOG(logDEBUG) << "\n Reading Non targets from file: " << nonTargetsFileName;
    }
    
    IMResults::getInstance().setFromFile(fromFile);
    // insert code here...
    float percentageTargetsFloat = (float)percentageTargets/(float)100;
    
    //Generate graph
    Graph *graph = new Graph;
    bool halfGraph=false;
    if(seedSelection.compare("bestHalfGraph")==0){
        graph->readHalfGraph(graphFileName, percentageTargetsFloat);
        seedSelection="bestTim";
        halfGraph=true;
    }
    
    else{
        graph->readGraph(graphFileName, percentageTargetsFloat);
    }
    if(!useIndegree) {
        graph->setPropogationProbability(probability);
    }
    
    //Start phase 1
    set<int> seedSet;
    
    if(seedSelection.compare("bestTim")==0){
        seedSet=runTim(graph,fromFile,nonTargetsFileName,method,budget,nonTargetThreshold, graphFileName, percentageTargets);
        vector<int>().swap(graph->NodeinRRsetsWithCounts);
        //checkMod(graphFileName, percentageTargetsFloat, graph, seedSet,budget, useIndegree,probability);
    }
    
    else{
        SeedSet *SeedClass=new SeedSet(graph , budget);
        //seed set selected randomly
        if(seedSelection.compare("random")==0){
            seedSet=SeedClass->getCompletelyRandom();
        }
        //seed set on best outdegree
        else if(seedSelection.compare("randomOutDegree")==0){
            seedSet=SeedClass->outdegreeRandom(topBestThreshold,set<int>(),set<int>());
        }
        
        else if(seedSelection.compare("farthestOutDegree")==0){
            seedSet=SeedClass->outdegreeFarthest(topBestThreshold);
        }
        delete SeedClass;
    }
    
    cout<<"\n Selected k sub SeedSet: " << flush;
    for(auto item:seedSet)
        cout<< item << " ";
    
    if(halfGraph){
        cout<<" Creating the complete graph again for diffusion: ";
        graph->readGraph(graphFileName, percentageTargetsFloat);
        if(!useIndegree) {
            graph->setPropogationProbability(probability);
        }
    }
    //Start Diffusion
    cout<< "\n Diffusion on graph started"<< flush;
    vector<int> activatedSet=performDiffusion(graph,seedSet,NULL);
    vector<vector<int>>().swap(graph->rrSets);
    delete graph;
    
    cout<< "\n Creating Influenced Graph "<< flush;
    Graph *influencedGraph = new Graph;
    influencedGraph->readInfluencedGraph(graphFileName, percentageTargetsFloat,activatedSet);
    
    if(!useIndegree) {
        influencedGraph->setPropogationProbability(probability);
    }
    cout << "\n Targets activated = " << activatedSet.size();
    cout << "\n Non targets are = " << influencedGraph->getNumberOfNonTargets()<< flush;
    
    
    //get node to be removed
    set<int> modNodesToremove;
    set<int> alreadyinSeed= set<int>();
    
    if(modular.compare("modular")==0){
        cout << "\n ******* Running modular approach ******** \n" <<flush;
        clock_t ModReverseStartTime = clock();
        modNodesToremove= removeVertices(influencedGraph,graph, removeNodes, seedSet, activatedSet,modular,alreadyinSeed);
        clock_t ModReverseEndTime = clock();
        
        //remove nodes from graph
        Graph *newGraph = new Graph;
        newGraph->readGraph(graphFileName, percentageTargetsFloat);
        
        if(!useIndegree) {
            newGraph->setPropogationProbability(probability);
        }
        
        double totalAlgorithmTime = double(ModReverseEndTime-ModReverseStartTime) / (CLOCKS_PER_SEC*60);
        cout << "\n Reverse algorithm time in minutes " << totalAlgorithmTime<<flush;
        
    }
    //else{
    cout << "\n ******* Running Sub Modular approach ******** \n" <<flush;
    clock_t subModReverseStartTime = clock();
    set<int> subModNodesToremove;
    //Random RR sets
    int n = (int)activatedSet.size();
    double epsilon = (double)EPSILON;
    int R = (8+2 * epsilon) * n * (2 * log(n) + log(2))/(epsilon * epsilon);
    cout<< "RR sets are: "<<R;
    influencedGraph->generateRandomRRSetsFromTargets(R, activatedSet,"submodular");
    //int node=getMarginalLoss(influencedGraph,activatedSet,nodesToremove);
    
    while(removeNodes!=0){
        //int node=removeVerticesIterative(influencedGraph,activatedSet,modular);
        //int node=modifiedremoveVerticesIterative(influencedGraph,activatedSet,nodesToremove,modular);
        
        vector<pair<int,int>> SortedNodeidCounts=vector<pair<int,int>>();
        for(int i=0;i<influencedGraph->coverage.size();i++){
            pair<int,int> node= pair<int,int>();
            node.first=i;
            node.second=influencedGraph->coverage[i];
            SortedNodeidCounts.push_back(node);
        }
        std :: sort(SortedNodeidCounts.begin(),SortedNodeidCounts.end(), sortbysecdesc);
        assert(SortedNodeidCounts.at(0).second>=SortedNodeidCounts.at(1).second);
        int h=0;
        while(seedSet.count(SortedNodeidCounts.at(h).first)==1){
            h++;
        }
        int node = SortedNodeidCounts.at(h).first;
        subModNodesToremove.insert(node);
        if(seedSet.count(node)==1){
            alreadyinSeed.insert(node);
        }
        //remove node from RRset
        influencedGraph->removeNodeFromRRset(node);
        removeNodes--;
    }
    //}
    cout << "\n Number of nodes Already present in seed set = " << alreadyinSeed.size();
    clock_t subModReverseEndTime = clock();
    
    //remove nodes from graph
    Graph *newGraph = new Graph;
    newGraph->readGraph(graphFileName, percentageTargetsFloat);
    
    if(!useIndegree) {
        newGraph->setPropogationProbability(probability);
    }
    
    //remove nodes from graph
    Graph *subNewGraph = new Graph;
    subNewGraph->readGraph(graphFileName, percentageTargetsFloat);
    
    if(!useIndegree) {
        subNewGraph->setPropogationProbability(probability);
    }
    newDiffusion(newGraph,subNewGraph,modNodesToremove,subModNodesToremove,activatedSet,budget,topBestThreshold);
    
    double totalAlgorithmTime = double(subModReverseEndTime-subModReverseStartTime) / (CLOCKS_PER_SEC*60);
    cout << "\n Reverse algorithm time in minutes " << totalAlgorithmTime;
    clock_t executionTimeEnd = clock();
    double totalExecutionTime = double(executionTimeEnd - executionTimeBegin) / (CLOCKS_PER_SEC*60);
    cout << "\n Elapsed time in minutes " << totalExecutionTime;
    
}


string constructResultFileName(string graphFileName, int budget, int nonTargetThreshold, int percentageTargets, ApproximationSetting setting) {
    string resultFileName = "results/" + graphFileName;
    resultFileName+="_" + to_string(budget);
    resultFileName+="_" + to_string(nonTargetThreshold);
    resultFileName+="_" + to_string(percentageTargets);
    resultFileName+="_" + to_string(rand() % 1000000);
    resultFileName+="_" + to_string(setting);
    resultFileName+=".json";
    return resultFileName;
}

void executeDifferenceAlgorithms(cxxopts::ParseResult result) {
    cout << "\n Executing difference" << flush;
    int budget = result["budget"].as<int>();
    string graphFileName = result["graph"].as<std::string>();
    int percentageTargets = result["percentage"].as<int>();
    float percentageTargetsFloat = (float)percentageTargets/(float)100;
    ApproximationSetting setting = static_cast<ApproximationSetting>(result["approximation"].as<int>());
    bool extendPermutation = false;
    if(result.count("extend")>0) {
        extendPermutation = result["extend"].as<bool>();
    }
    cout << "\n Conducting experiments for:\n";
    cout <<" Graph: " << graphFileName;
    cout << "\t Budget: " << budget;
    cout << "\t Percentage:  " << percentageTargets;
    cout << "\t Approximation setting: " << setting;
    cout << "\t Extend: " << extendPermutation;
    cout << flush;
    
    Graph *graph = new Graph;
    graph->readGraph(graphFileName, percentageTargetsFloat);
    //    Begin f-g
    clock_t differenceStartTime = clock();
    
    
    testApprox(graph, budget, setting, extendPermutation);
    clock_t differenceEndTime = clock();
    double differenceTimeTaken = double(differenceEndTime - differenceStartTime) / CLOCKS_PER_SEC;
    IMResults::getInstance().setApproximationTime(differenceTimeTaken);
    IMResults::getInstance().setApproximationSetting(setting);
    IMResults::getInstance().setExtendingPermutation(extendPermutation);
    // Setting 1000 as NT threshold. Actually not applicable. TODO: do this better.
    string resultFile = constructResultFileName(graphFileName, budget, 1000, percentageTargets, setting);
    IMResults::getInstance().writeToFile(resultFile);
}

void executeTIMOnLabelledGraph(cxxopts::ParseResult result) {
    int budget = result["budget"].as<int>();
    string graphFileName = result["graph"].as<std::string>();
    int percentageTargets = result["percentage"].as<int>();
    float percentageTargetsFloat = (float)percentageTargets/(float)100;
    Graph *unlabelledGraph = new Graph;
    unlabelledGraph->readGraph(graphFileName, 1.0f);
    int n = unlabelledGraph->n;
    double epsilon = (double)EPSILON;
    int R = (8+2 * epsilon) * n * (2 * log(n) + log(2))/(epsilon * epsilon);
    //    R = 23648871;
    unlabelledGraph->generateRandomRRSets(R, true);
    vector<vector<int>>* rrSets = unlabelledGraph->getRandomRRSets();
    
    vector<vector<int>> lookupTable;
    TIMCoverage *timCoverage = new TIMCoverage(&lookupTable);
    timCoverage->initializeLookupTable(rrSets, n);
    timCoverage->initializeDataStructures(R, n);
    timCoverage->offsetCoverage(0, -10);
    // 0 should not be the top
    set<int> topNodes = timCoverage->findTopKNodes(budget, rrSets,unordered_set<int>());
    delete timCoverage;
    
    Graph *labelledGraph = new Graph;
    labelledGraph->readGraph(graphFileName, percentageTargetsFloat);
    pair<int, int> influence = findInfluenceUsingDiffusion(labelledGraph, topNodes);
    int targetsActivated = influence.first;
    int nonTargetsActivated = influence.second;
    
    cout << "\n Targets activated = " << targetsActivated;
    cout << "\n Non targets activated = " << nonTargetsActivated;
    IMResults::getInstance().setExpectedTargets(influence);
    string resultFile = constructResultFileName(graphFileName, budget, 1000, percentageTargets, setting1);
    IMResults::getInstance().writeToFile(resultFile);
}

void executeBaselineGreedy(cxxopts::ParseResult result) {
    cout << "\n Executing baseline greedy" << flush;
    int budget = result["budget"].as<int>();
    string graphFileName = result["graph"].as<std::string>();
    int percentageTargets = result["percentage"].as<int>();
    float percentageTargetsFloat = (float)percentageTargets/(float)100;
    int nonTargetThreshold = result["threshold"].as<int>();
    
    loadResultsFileFrom(result);
    
    Graph *graph = new Graph;
    graph->readGraph(graphFileName, percentageTargetsFloat);
    if(result.count("p")>0) {
        double probability = result["p"].as<double>();
        graph->setPropogationProbability(probability);
    }
    loadGraphSizeToResults(graph);
    
    clock_t baselineStartTime = clock();
    BaselineGreedyTIM baselineGreedyTIM;
    set<int> seedSet = baselineGreedyTIM.findSeedSet(graph, budget, nonTargetThreshold);
    clock_t baselineEndTime = clock();
    double baselineTimeTaken = double(baselineEndTime - baselineStartTime) / CLOCKS_PER_SEC;
    
    
    TIMInfluenceCalculator  timInfluenceCalculator(graph, 2);
    pair<int, int> influence = timInfluenceCalculator.findInfluence(seedSet);
    cout <<"\n Results after Diffusion: ";
    cout << "\nInfluence Targets: " << influence.first;
    cout << "\nInfluence NT: " << influence.second;
    FILE_LOG(logDEBUG) << "\n Time Taken: " << baselineTimeTaken;
    vector<int> orderedSeed = baselineGreedyTIM.getOrderedSeed();
    set<int> greedySeedSet;
    vector<IMSeedSet> allSeedSets;
    for (int i=(int)(orderedSeed.size()-1); i<orderedSeed.size(); i++) {
        greedySeedSet.insert(orderedSeed[i]);
        TIMInfluenceCalculator  timInfluenceCalculatorGreedy(graph, 2);
        pair<int, int> seedSetInfluence = timInfluenceCalculatorGreedy.findInfluence(greedySeedSet);
        IMSeedSet imSeedSet;
        vector<int> seedVector(orderedSeed.begin(), orderedSeed.begin() + i + 1);
        // Reverse this before adding so the last seed is first
        reverse(seedVector.begin(), seedVector.end());
        for (int s: seedVector) {
            imSeedSet.addSeed(s);
        }
        imSeedSet.setTargets(seedSetInfluence.first);
        imSeedSet.setNonTargets(seedSetInfluence.second);
        allSeedSets.push_back(imSeedSet);
    }
    IMResults::getInstance().addSeedSets(allSeedSets);
    IMResults::getInstance().addBestSeedSet(allSeedSets[0]);
    
    IMResults::getInstance().setTotalTimeTaken(baselineTimeTaken);
    string resultFile = constructResultFileName(graphFileName, budget, nonTargetThreshold, percentageTargets, setting1);
    IMResults::getInstance().setExpectedTargets(influence);
    IMResults::getInstance().writeToFile(resultFile);
    delete graph;
}

void generateGraphLabels(cxxopts::ParseResult result) {
    string graphFileName = result["graph"].as<std::string>();
    int percentageTargets = result["percentage"].as<int>();
    float percentageTargetsFloat = (float)percentageTargets/(float)100;
    Graph *graph = new Graph;
    graph->readGraph(graphFileName, percentageTargetsFloat);
    GenerateGraphLabels(graph, percentageTargetsFloat);
}

int main(int argc, char **argv) {
    cout << "Starting program\n";
    srand(time(0));
    setupLogger();
    cout << "Setup logger \n";
    cxxopts::Options options("Targeted Influence Maximization", "Experiments and research.");
    options.add_options()
    ("algorithm", "Choose which algorithm to run", cxxopts::value<std::string>())
    ("graph", "Graph file name", cxxopts::value<std::string>())
    ("b,budget", "Budget size", cxxopts::value<int>())
    ("t,threshold", "NT threshold", cxxopts::value<int>())
    ("m,method", "TIM-TIM or SIM-SIM", cxxopts::value<int>())
    ("percentage", "Percentage of Targets", cxxopts::value<int>())
    ("n,ntfile", "Non Targets File name", cxxopts::value<std::string>())
    ("p,probability", "Propogation probability", cxxopts::value<double>())
    ("approximation", " Approximation Settings", cxxopts::value<int>())
    ("r,nodesRemove", " Remove nodes number", cxxopts::value<int>())
    ("w,modularity", " Modular selection", cxxopts::value<std::string>())
    ("s,seedset", " Random seed set", cxxopts::value<std::string>())
    ("e,extend", "Extend the permutation");
    auto result = options.parse(argc, argv);
    string algorithm = result["algorithm"].as<string>();
    if(result["algorithm"].count()>0 && algorithm.compare("generate")==0) {
        generateGraphLabels(result);
    } else if(result["algorithm"].count()>0 && algorithm.compare("timtim")==0) {
        executeTIMTIM(result);
    } else if(result["algorithm"].count()>0 && algorithm.compare("tim")==0) {
        cout << "\n Executing just TIM";
        executeTIMOnLabelledGraph(result);
        
    } else if(result["algorithm"].count()>0 && algorithm.compare("baseline")==0 ) {
        executeBaselineGreedy(result);
    }else {
        executeDifferenceAlgorithms(result);
    }
    
    disp_mem_usage("");
    return 0;
}
