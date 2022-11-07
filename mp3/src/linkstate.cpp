#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mp3.h"
#include <iostream>
#include <fstream>
#include <queue>
#include <climits>

void createEdge(int u, int v, int w, vector<Pair> adj[]) {
    adj[u].push_back(make_pair(v, w));
    adj[v].push_back(make_pair(u, w));
}

void deleteEdge(int u, int v, vector<Pair> adj[]) {
    for (int i = 0; i < adj[u].size(); i++) {
        if (adj[u][i].first == v) {
            adj[u].erase(adj[u].begin() + i);
            break;
        }
    }
    for (int i = 0; i < adj[v].size(); i++) {
        if (adj[v][i].first == u) {
            adj[v].erase(adj[v].begin() + i);
            break;
        }
    }
}

void dijkstras(vertex* vx, int numVertices, vector<Pair> adj[]) {
    if (NULL == vx) {
        printf("Invalid vertex\n");
        return;
    }
    // cout << "vertex " << vx->sourceID << endl;
    // for (int i = 0; i < numVertices; i++){
    //     for (int j = 0; j < adj[i].size(); j++){
    //         cout << i << " " << adj[i][j].first << " " << adj[i][j].second << endl;
    //     }
    // }
    priority_queue<Pair, vector<Pair>, greater<Pair> > pq;
    vector<bool> visited(numVertices, false);
    vx->dist.clear();
    vx->prev.clear();
    vx->dist.resize(numVertices + 1, INT_MAX);
    vx->prev.resize(numVertices + 1, -1);
    vx->dist[vx->sourceID] = 0;
    pq.push(make_pair(0, vx->sourceID));
    
    while (!pq.empty()) {
        int u = pq.top().second;
        pq.pop();
        // Prevent cycling
        if (visited[u]) continue;
        visited[u] = true;
        for (int i = 0; i < adj[u].size(); i++) {
            int v = adj[u][i].first;
            int w = adj[u][i].second;
            if (vx->dist[v] > vx->dist[u] + w) {
                vx->dist[v] = vx->dist[u] + w;
                vx->prev[v] = u;
                pq.push(make_pair(vx->dist[v], v));
            }
            // Tie breaking
            else if (vx->dist[v] == vx->dist[u] + w) {
                if (vx->prev[v] > u) {
                    vx->prev[v] = u;
                    pq.push(make_pair(vx->dist[v], v));
                }
            }
        }
    }

    if (vx->sourceID == 3){
        for (int i = 0; i < vx->dist.size(); i++){
            cout << vx->dist[i] << " ";
        }
        cout << endl;
        for (int i = 0; i < vx->prev.size(); i++){
            cout << vx->prev[i] << " ";
        }
        cout << endl;
    }

}

vector<int> findPath(int src, int dest, vertex* vertices) {
    int nexthop = dest;
    vector<int> path;
    
    if (vertices[src].prev[nexthop] == src){
        path.push_back(dest);
    }
    while (vertices[src].prev[nexthop] != src){
        if (vertices[src].prev[nexthop] == -1){
            nexthop = src;
            path.push_back(nexthop);
            break;
        }
        nexthop = vertices[src].prev[nexthop];
        path.push_back(nexthop);
    }
    
    return path;
}

bool updateWeight(int u, int v, int w, vector<Pair> adj[]) {
    bool found = 0;
    for (int i = 0; i < adj[u].size(); i++) {
        if (adj[u][i].first == v) {
            adj[u][i].second = w;
            found = 1;
            break;
        }
    }
    for (int i = 0; i < adj[v].size(); i++) {
        if (adj[v][i].first == u) {
            adj[v][i].second = w;
            found = 1;
            break;
        }
    }
    return found;
}

void printRoutingTable(FILE* fpOut, int numVertices, vertex* vertices) {
    for (int src = 1; src <= numVertices; ++src) {
        for (int dest = 1; dest <= numVertices; ++dest) {
            if (vertices[src].dist[dest] != INT_MAX) {
                // Find the path
                vector<int> path = findPath(src, dest, vertices);
                fprintf(fpOut, "%d %d %d\n", dest, path.back(), vertices[src].dist[dest]);
            }
        }
    }
}

void printOneLineMsg(FILE* fpOut, string line, vertex* vertices) {
    int src = atoi(line.substr(0, line.find(" ")).c_str());
    int secondSpaceIdx = line.find(" ") + 1;
    int dest = atoi(line.substr(line.find(" ") + 1, line.find(" ", secondSpaceIdx)).c_str());
    string msg = line.substr(line.find(" ", secondSpaceIdx) + 1);

    if (vertices[src].dist[dest] != INT_MAX) {
        // Find the path
        vector<int> path = findPath(src, dest, vertices);
        path.push_back(src); // need to append src here, but no need when calculating prev!!!
        fprintf(fpOut, "from %d to %d cost %d hops ", src, dest, vertices[src].dist[dest]);
        for (int i = path.size() - 1; i >= 0; --i) {
            fprintf(fpOut, "%d ", path[i]);
        }
        fprintf(fpOut, "message %s\n", msg.c_str());
    }
    // Unreachable case
    else {
        fprintf(fpOut, "from %d to %d cost infinite hops unreachable message %s\n", src, dest, msg.c_str());
        return;
    }
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    ifstream inFile(argv[1]);
    string src, dest, weight;
    
    // Get the number of vertices in the topology
    int numVertices = 0;
    while(inFile >> src >> dest >> weight){
        if (atoi(src.c_str()) > numVertices) numVertices = atoi(src.c_str());
        if (atoi(dest.c_str()) > numVertices) numVertices = atoi(dest.c_str());
    }
    inFile.close();

    // Read the topology file and create the graph
    inFile.open(argv[1]);
    vector<Pair> adj[numVertices + 1]; 
    while(inFile >> src >> dest >> weight){
        if (atoi(weight.c_str()) == -999) continue;
        createEdge(atoi(src.c_str()), atoi(dest.c_str()), atoi(weight.c_str()), adj);
    }
    inFile.close();

    // Create the vertices
    vertex* vertices = new vertex[numVertices + 1];
    for (int i = 1; i <= numVertices; ++i) {
        vertices[i].sourceID = i;
        vertices[i].dist.resize(numVertices + 1, INT_MAX);
        vertices[i].prev.resize(numVertices + 1, -1);
    }

    // Run Dijkstra's algorithm on each vertex
    for (int i = 1; i <= numVertices; ++i) {
        dijkstras(&vertices[i], numVertices, adj);
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");

    // Initial routing table
    printRoutingTable(fpOut, numVertices, vertices);
    
    // Read the message file and print
    inFile.open(argv[2]);
    string line;
    while(getline(inFile, line)){
        printOneLineMsg(fpOut, line, vertices);
    }
    inFile.close();

    // Read the changes file and print
    ifstream changesFile(argv[3]);
    string tmpSrc, tmpDest, tmpWeight;
    while(changesFile >> tmpSrc >> tmpDest >> tmpWeight){
        int src = atoi(tmpSrc.c_str());
        int dest = atoi(tmpDest.c_str());
        int weight = atoi(tmpWeight.c_str());

        // Update the weight on the edge
        if (weight != -999){
            // weight = LARGE_WEIGHT;
            bool found = updateWeight(src, dest, weight, adj);
            if (!found && weight != LARGE_WEIGHT){
                createEdge(src, dest, weight, adj);
            }
        }
        else{
            deleteEdge(src, dest, adj);
        }

        // Run Dijkstra's algorithm on each vertex
        for (int i = 1; i <= numVertices; ++i) {
            dijkstras(&vertices[i], numVertices, adj);
        }
        // Print the new routing table
        printRoutingTable(fpOut, numVertices, vertices);

        // Print the new messages
        inFile.open(argv[2]);
        while(getline(inFile, line)){
            printOneLineMsg(fpOut, line, vertices);
        }
        inFile.close();
    }
    changesFile.close();

    fclose(fpOut);
    return 0;
}

