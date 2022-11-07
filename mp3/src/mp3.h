#ifndef MP3_H
#define MP3_H


#include <functional>
#include <vector>
using namespace std;

typedef pair<int, int> Pair;

struct vertex{
    int sourceID;
    vector<int> dist;
    vector<int> prev;
};

#endif