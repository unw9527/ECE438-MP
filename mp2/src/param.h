#ifndef PARAM_H
#define PARAM_H

#include <stdlib.h>

#define MSS             1000
#define BASE            2000
#define AMPLIFIER       128
#define INIT_SST_AMP    64
#define MAX_QUEUE_SIZE  1000
#define TIMEOUT         100000

enum packet_t{
    DATA,
    ACK,
    FIN,
    FINACK
};

enum status_t{
    SLOW_START = 10,
    CONGESTION_AVOID,
    FAST_RECOVERY
};

typedef struct{
	int 	    data_size;
	uint64_t 	seq_idx;
	uint64_t    ack_idx;
	packet_t    pkt_type;
	char        data[MSS];
}packet;

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

