/* 
 * File:   receiver_main.cpp
 * Author: Kunle Li
 *
 * Created on Oct 15, 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <queue>
#include "param.h"

using namespace std;

priority_queue<packet, vector<packet>, compare> pqueue;

struct sockaddr_in si_me, si_other;
int s, slen;


/**
 * @brief Helper function to send ACK
 * 
 * @param ack_idx   ACK index 
 * @param pkt_type  Packet type, one of DATA, ACK, FIN, FINACK
 */
void send_ack(int ack_idx, packet_t pkt_type){
    packet ack;
    ack.ack_idx = ack_idx;
    ack.pkt_type = pkt_type;

    if (sendto(s, &ack, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t)sizeof (si_other))==-1){
        diep("ACK's sendto()");
    }
}

/**
 * @brief Main receiver function
 * 
 * @param myUDPport         UDP port number 
 * @param destinationFile   File to write the received data to
 */
void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);
    int ack_idx = 0;

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */ 
    FILE* fp = fopen(destinationFile, "wb");  
    if (fp == NULL){
        diep("Cannot open the destination file");
    } 
    while (true){
        packet recv_pkt;
        if (recvfrom(s, &recv_pkt, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t*)&slen) == -1)
            diep("recvfrom()");

        /* The end of transmission */
        if (recv_pkt.pkt_type == FIN){
            cout << "FIN received" << endl;
            send_ack(ack_idx, FINACK);
            break;
        }
        else if (recv_pkt.pkt_type == DATA){
            /* Stale packets */
            if (recv_pkt.seq_idx < ack_idx){
                // cout << "Received duplicate packet " << recv_pkt.seq_idx << endl;
            }

            /* Out of order packets */
            else if (recv_pkt.seq_idx > ack_idx){
                // cout << "Received out of order packet " << recv_pkt.seq_idx << endl;
                // cout << "The queue size is " << pqueue.size() << endl;
                if (pqueue.size() < MAX_QUEUE_SIZE){
                    pqueue.push(recv_pkt);
                }
                else{
                    cout << "Queue is full" << endl;
                }
            }

            /* In order packets */
            else{
                // cout << "Received in order packet " << recv_pkt.seq_idx << endl;
                /* Write to the destination file */
                fwrite(recv_pkt.data, sizeof(char), recv_pkt.data_size, fp);
                ack_idx += recv_pkt.data_size;

                /* Write packets that are at the top of the queue */
                while (!pqueue.empty() && pqueue.top().seq_idx == ack_idx){
                    packet pkt = pqueue.top();
                    fwrite(pkt.data, sizeof(char), pkt.data_size, fp);
                    ack_idx += pkt.data_size;
                    pqueue.pop();
                }
            }
            send_ack(ack_idx, ACK);
        }
    }
    fclose(fp);
    close(s);
	printf("%s received.", destinationFile);
    return;
}


int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

