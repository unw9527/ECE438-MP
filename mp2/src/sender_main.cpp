/* 
 * File:   sender_main.cpp
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <queue>
#include <math.h>
#include <errno.h>
#include "param.h"
#include <cstdio>

using namespace std;


struct sockaddr_in si_me, si_other;
int s, slen;

FILE* fp;
uint64_t num_total_pkt;
uint64_t bytes_to_send;
uint64_t num_sent;
uint64_t num_received;
uint64_t num_dup;
uint64_t seq_idx;
status_t status;
float cwnd;
float ssthresh;

queue <packet> backup_queue;
queue <packet> trial_queue;


/**
 * @brief Helper function to send packets
 * 
 * @param pkt   Packet to be sent
 */ 
void send_pkt(packet* pkt){
    // cout << "Sending packet " << pkt->seq_idx << endl;
    if (sendto(s, pkt, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))== -1){
        diep("sendto()");
    }
}

/**
 * @brief Helper function to handle timeout
 * 
 */
void timeout_handler(){
    // cout << "Timeout!" << endl;
    ssthresh = cwnd / 2;
    ssthresh = max((float)MSS * AMPLIFIER, ssthresh);
    cwnd = MSS * AMPLIFIER;
    // cout << "timeout window size: " << cwnd << " ssthresh: " << ssthresh << endl;
    status = SLOW_START;

    queue<packet> tmp_q = backup_queue; 
    for (int i = 0; i < 32; i++){
        if (!tmp_q.empty()){
            send_pkt(&tmp_q.front());
            tmp_q.pop();
        }
    }
    num_dup = 0;
}

/**
 * @brief Set ssthresh and cwnd when num_dup >= 3
 * 
 */
void dup_ack_handler(){
    ssthresh = cwnd / 2;
    ssthresh = max((float)MSS, ssthresh);
    cwnd = ssthresh + 3 * MSS;
    cwnd = max((float)MSS, cwnd);
    // cout << "duplicate ack window size: " << cwnd << " ssthresh: " << ssthresh << endl;
    status = FAST_RECOVERY;
    if (!backup_queue.empty()){
        send_pkt(&backup_queue.front());
    }
    num_dup = 0;
}

/**
 * @brief Transit among three states for dynamic windowing
 * 
 */
void state_transition(){
    switch(status){
        case SLOW_START:
            // Duplicate ACK
            if (num_dup >= 3){
                dup_ack_handler();
            }
            // New ACK
            else if (num_dup == 0){
                if (cwnd >= ssthresh){
                    status = CONGESTION_AVOID;
                    return;
                }
                cwnd += MSS;
                cwnd = max((float)MSS, cwnd);
                // cout << "SLOW_START window size: " << cwnd << " ssthresh: " << ssthresh << endl;
            }
            break;
        case CONGESTION_AVOID:
            // Duplicate ACK
            if (num_dup >= 3){
                dup_ack_handler();
            }
            // New ACK
            else if (num_dup == 0){
                cwnd += MSS * floor(1.0 * MSS / cwnd); 
                cwnd = max((float)MSS, cwnd);
                // cout << "CONGESTION_AVOID window size: " << cwnd << " ssthresh: " << ssthresh << endl;
            }
            break;
        case FAST_RECOVERY:
            // Duplicate ACK
            if (num_dup > 0){
                cwnd += MSS;
                return;
            }
            // New ACK
            else if (num_dup == 0){
                cwnd = ssthresh;
                cwnd = max((float)MSS, cwnd);
                // cout << "FAST_RECOVERY window size: " << cwnd << " ssthresh: " << ssthresh << endl;
                status = CONGESTION_AVOID;
            }
            break;
        default: 
            break;
    }
}

/**
 * @brief Enqueue packets to both queues and send 
 *        all packets for the first trial
 * 
 */
void enqueue_and_send(){
    if (bytes_to_send == 0){
        return;
    }
    char buf[MSS];
    memset(buf, 0, MSS);
    packet pkt;
    for (int i = 0; i < ceil((cwnd - backup_queue.size() * MSS) / MSS); i++){
        int temp = min(bytes_to_send, (uint64_t)MSS); // Need to ensure that the read size is not bigger than 
                                                      // #bytes we want to send!!!!!!
        int read_size = fread(buf, sizeof(char), temp, fp);
        if (read_size > 0){
            pkt.pkt_type = DATA;
            pkt.data_size = read_size;
            pkt.seq_idx = seq_idx;
            memcpy(pkt.data, &buf, read_size);
            backup_queue.push(pkt);
            trial_queue.push(pkt);
            seq_idx += read_size;
            bytes_to_send -= read_size;
        }
    }
    // Send all packets for the first trial
    while (!trial_queue.empty()){
        send_pkt(&trial_queue.front());
        num_sent++;
        trial_queue.pop();
    }
}

/**
 * @brief Handle new and duplicate ACKs
 * 
 * @note  This function calls state_transition() to transit among three states. 
 *        Need to set num_dup according to the TCP state diagram in this function
 * 
 * @param pkt   Received packet
 */
void ack_handler(packet* pkt){
    if (pkt->ack_idx < backup_queue.front().seq_idx){
        // Stale ACK
        return;
    }
    else if (pkt->ack_idx == backup_queue.front().seq_idx){
        // Duplicate ACK
        num_dup++;
        state_transition();
    }
    else{
        // New ACK
        num_dup = 0;
        state_transition();
        int num_pkt = ceil((pkt->ack_idx - backup_queue.front().seq_idx) / (1.0 * MSS));
        int count = 0;
        num_received += num_pkt;
        while(!backup_queue.empty() && count < num_pkt){
            backup_queue.pop();
            count++;
        }
        enqueue_and_send();
    }
}

/**
 * @brief End connection when the file is finished being transmitted
 * 
 */
void end_connection(){
    packet pkt;
    char temp[sizeof(packet)];
    pkt.pkt_type = FIN;
    pkt.data_size = 0;
    memset(pkt.data, 0, MSS);
    cout << "Sending FIN" << endl;
    send_pkt(&pkt);
    while (true){
        slen = sizeof(si_other);
        if (recvfrom(s, temp, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t*)&slen) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK){
                diep("recvfrom()");
            }
            else{
                cout << "Timeout. Resend FIN" << endl;
                pkt.pkt_type = FIN;
                pkt.data_size = 0;
                memset(pkt.data, 0, MSS);
                send_pkt(&pkt);
            }
        }
        else{
            packet ack;
            memcpy(&ack, temp, sizeof(packet));
            if (ack.pkt_type == FINACK){
                cout << "Received FINACK" << endl;
                pkt.pkt_type = FINACK;
                pkt.data_size = 0;
                send_pkt(&pkt);
                cout << "DONE" << endl;
                break;
            }
        }
    }
}

/**
 * @brief Main function for sending packets
 * 
 * @param hostname          Hostname of the receiver
 * @param hostUDPport       UDP port number
 * @param filename          File to be sent
 * @param bytesToTransfer   #bytes to tranfer
 */
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    /* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Open the file */
    fp = fopen(filename, "rb");
    if (fp == NULL){
        diep("sender: fopen");
    }
    
    /* Initialize var */
    num_total_pkt = ceil(1.0 * bytesToTransfer / MSS);
    bytes_to_send = bytesToTransfer;
    seq_idx = 0;
    num_sent = 0;
    num_received = 0;
    num_dup = 0;
    status = SLOW_START;
    cwnd = MSS;
    ssthresh = cwnd * float(INIT_SST_AMP);

    /* Set timeout for the socket */
    timeval RTO;
    RTO.tv_sec = 0;
    RTO.tv_usec = TIMEOUT;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &RTO, sizeof(RTO)) == -1){
        diep("setsockopt failed");
    }

    /* Send data and receive acknowledgements on s */
    packet pkt;
    enqueue_and_send();
    while (num_sent < num_total_pkt || num_received < num_total_pkt){
        // cout << "num_sent: " << num_sent << " num_total: " << num_total_pkt << " num_received: " << num_received << endl;
        if ((recvfrom(s, &pkt, sizeof(packet), 0, NULL, NULL)) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("recvfrom()");
            }
            if (!backup_queue.empty()){
                // Timeout
                timeout_handler();
            }
        }
        else{
            if (pkt.pkt_type == ACK){
                ack_handler(&pkt);
            }
        }
    }
    end_connection();
    fclose(fp);
    return;
}

int main(int argc, char** argv) {
    unsigned short int udpPort;
    unsigned long long int numBytes;
    // freopen("output.txt", "w", stdout); // Debugging purpose

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}


