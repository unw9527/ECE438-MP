#ifndef MP3_H
#define MP3_H


#include <functional>
#include <vector>
using namespace std;

#define LARGE_WEIGHT    99999

typedef pair<int, int> Pair;

struct vertex{
    int sourceID;
    vector<int> dist;
    vector<int> prev;
};

#endif