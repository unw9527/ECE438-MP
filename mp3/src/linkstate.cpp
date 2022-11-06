#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mp3.h"
#include <iostream>
#include <fstream>
#include <queue>

void createEdge(int u, int v, int w, vector<Pair> adj[]) {
    adj[u].push_back(make_pair(v, w));
    adj[v].push_back(make_pair(u, w));
}

void dijkstras(vertex* vx, int numVertices, vector<Pair> adj[]) {
    if (NULL == vx) return;
    priority_queue<Pair, vector<Pair>, greater<Pair> > pq;
    vector<bool> visited(numVertices, false);
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
        }
    }

}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
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

    // Read the topology file
    inFile.open(argv[1]);
    vector<Pair> adj[numVertices + 1]; 
    while(inFile >> src >> dest >> weight){
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
    for (int src = 1; src <= numVertices; ++src) {
        for (int dest = 1; dest <= numVertices; ++dest) {
            if (vertices[src].dist[dest] != INT_MAX) {
                fprintf(fpOut, "%d %d %d\n", dest, vertices[src].prev[dest], vertices[src].dist[dest]);
            }
        }
    }

    fclose(fpOut);
    return 0;
}

