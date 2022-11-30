#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fstream>
#include <map>
#include <iostream>
#include <algorithm>
#include <vector>
using namespace std;

typedef struct{
    int id, Rlevel, backoff;
} node_t;

typedef struct{
    int N, L, M, trans_state;
    unsigned long T, current, success_trans;
    vector<int> R;
    vector<node_t> hosts;
} csma_t;

static csma_t csma;

void read_file(string file){
    csma.trans_state = 0;
    csma.current = 0;
    csma.success_trans = 0;
	ifstream infile;
	infile.open(file);
    string line;
    while (getline(infile, line)) {
        char* str = const_cast<char *>(line.c_str());
        long val;
        if (*str == 'N') sscanf(str + 1, "%d", &csma.N);
        else if (*str == 'L') sscanf(str + 1, "%d", &csma.L);
        else if (*str == 'M') sscanf(str + 1, "%d", &csma.M);
        else if (*str == 'T') sscanf(str + 1, "%lu", &csma.T);
        else if (*str == 'R') {
            val = strtol(str + 1, &str, 10);
            while (val) {csma.R.push_back(val); val = strtol(str, &str, 10);}
        }
    }

    for (int i = 0; i < csma.N; i++) {
        node_t host;
        host.id = i;
        host.Rlevel = 0;
        host.backoff = host.id % csma.R[host.Rlevel];
        csma.hosts.push_back(host);
    }

    infile.close();
}

int manipulate(){
    vector<int> c;
    for (int i = 0; i < csma.N; i++) if (csma.hosts[i].backoff == 0) c.push_back(i);
    if (c.size() == 1) {
        csma.hosts[c[0]].Rlevel = 0;
        csma.hosts[c[0]].backoff = (csma.hosts[c[0]].id + csma.current + csma.L) % csma.R[csma.hosts[c[0]].Rlevel];
        if (csma.current + csma.L < csma.T) {csma.current += csma.L - 1; csma.success_trans += csma.L;}
        else {csma.success_trans += csma.T - csma.current; csma.current = csma.T - 1;}
    }
    else if (c.size() > 1) {
        for (int i = 0; i < c.size(); i++) {
            if (++csma.hosts[c[i]].Rlevel == csma.M) csma.hosts[c[i]].Rlevel = 0;
            csma.hosts[c[i]].backoff = (csma.hosts[c[i]].id + csma.current + 1) % csma.R[csma.hosts[c[i]].Rlevel];
        }
    }
    else for (int i = 0; i < csma.N; i++) csma.hosts[i].backoff--;
    return csma.current++;
}


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }
    FILE* outfile = fopen("output.txt", "w");
    read_file(argv[1]);
    while(manipulate() != csma.T - 1);
    fprintf(outfile, "%.2lf\n", (double) csma.success_trans / (double) csma.T);
    fclose(outfile);
    return 0;
}
