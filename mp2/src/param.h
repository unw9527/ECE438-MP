#ifndef PARAM_H
#define PARAM_H

#include <stdlib.h>

#define MSS             4000
#define AMPLIFIER       256 // Amplify the ssthreash by this factor
#define INIT_SST_AMP    512 // Initial ssthreash is this factor times the MSS
#define MAX_QUEUE_SIZE  1000
#define TIMEOUT         100000

enum packet_t{
    DATA,
    ACK,
    FIN,
    FINACK
};

enum status_t{
    SLOW_START = 5,
    CONGESTION_AVOID,
    FAST_RECOVERY
};

typedef struct{
	int 	    data_size;
	uint64_t    ack_idx;
    uint64_t 	seq_idx;
	packet_t    pkt_type;
	char        data[MSS];
}packet;

/* This struct is for the priority queue */
struct compare {
    bool operator()(packet a, packet b) {
        return  a.seq_idx > b.seq_idx; 
    }
};

extern void diep(const char *s);

/**
 * @brief Reports errors and exits
 * 
 * @param s 
 */
void diep(const char *s) {
    perror(s);
    exit(1);
}

#endif

