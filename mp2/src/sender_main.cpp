/* 
 * File:   sender_main.c
 * Author: Kunle Li
 *
 * Created on 
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

using namespace std;

#define MSS             512
#define MAX_QUEUE_SIZE  512
#define TIMEOUT         40000
float cwnd = 1.0;
float ssthresh = 128.0;
int num_dup_pkt = 0;
unsigned long long int bytesToSend;
int seq_idx = 0;
bool new_ack;
FILE* fp;

enum packet_t {
    DATA,
    ACK,
    FIN,
    FINACK
};

enum status_t {
    SLOW_START,
    CONGESTION_AVOIDANCE,
    FAST_RECOVERY
};

status_t status = SLOW_START;

typedef struct{
    int         data_size;
	int 	    seq_idx;
	int         ack_idx;
	packet_t    pkt_type;
	char        data[MSS];
} packet;

queue <packet> dqueue;
queue <packet> aqueue;

struct sockaddr_in si_other;
int s, slen;

/**
 * @brief Reports errors and exits
 * 
 * @param s 
 */
void diep(const char *s) {
    perror(s);
    exit(1);
}

/**
 * @brief Enqueue a packet into the queue
 * 
 * @param num 
 */
void enqueue(){
    if (bytesToSend <= 0){
        return;
    }
    char buf[MSS];
    memset(buf, 0, MSS);
    packet pkt;
    for (int i = 0; i < ceil((cwnd - dqueue.size() * MSS) / MSS); i++){
        int read_size = fread(buf, sizeof(char), MSS, fp);
        if (read_size > 0){
            pkt.pkt_type = DATA;
            pkt.data_size = read_size;
            pkt.seq_idx = seq_idx;
            memcpy(pkt.data, &buf, read_size);
            seq_idx += read_size;
            bytesToSend -= read_size;
            dqueue.push(pkt);
        }
    }
}

/**
 * @brief Helper function to send packets
 * 
 */
void send_pkt() {
    cout << "Sending packet " << dqueue.front().seq_idx << endl;
    int num_send = cwnd - aqueue.size();
    enqueue();
    if (num_send < 1){
        if (sendto(s, &aqueue.front(), sizeof(packet), 0, (struct sockaddr *) &si_other, slen) == -1){
            diep("sendto()");
        }
    }
    else{
        if (!dqueue.empty()){
            num_send = min(num_send, int(dqueue.size()));
            for (int i = 0; i < num_send; i++){
                if (sendto(s, &dqueue.front(), sizeof(packet), 0, (struct sockaddr *) &si_other, slen) == -1){
                    diep("sendto()");
                }
                aqueue.push(dqueue.front());
                dqueue.pop();
            }
        }
        else{
            cout << "No more data to send" << endl;
        }
    }
}

/**
 * @brief State transition function for dynamic windowing
 * 
 */
void handle_status() {
    cout << "The current status is: " << status << endl;
    switch (status) {
        case SLOW_START:
            if (new_ack){
                new_ack = false;
                cwnd++;
                num_dup_pkt = 0;
                send_pkt();
            }
            else{
                num_dup_pkt++;
            }
            if (cwnd >= ssthresh){
                status = CONGESTION_AVOIDANCE;
                return;
            }
            if (num_dup_pkt == 3){
                status = FAST_RECOVERY;
                ssthresh = cwnd / 2;
                cwnd = ssthresh + 3;
                send_pkt();
                return;
            }
            break;
        case CONGESTION_AVOIDANCE:
            if (new_ack){
                new_ack = false;
                cwnd += 1.0 / cwnd;
                num_dup_pkt = 0;
                send_pkt();
            }
            else{
                num_dup_pkt++;
            }
            if (num_dup_pkt == 3){
                status = FAST_RECOVERY;
                ssthresh = cwnd / 2;
                cwnd = ssthresh + 3;
                send_pkt();
                return;
            }
            break;
        case FAST_RECOVERY:
            if (new_ack){
                new_ack = false;
                cwnd = ssthresh;
                status = CONGESTION_AVOIDANCE;
                return;
            }
            else{
                num_dup_pkt++;
                cwnd++;
                send_pkt();
            }
            break;
        default:
            break;
    }
}

/**
 * @brief End the connection by sending FIN
 * 
 */
void end_connection(){
    cout << "File transfer complete" << endl;
    packet fin_pkt;
    fin_pkt.pkt_type = FIN;
    fin_pkt.data_size = 0;
    memset(fin_pkt.data, 0, MSS);
    cout << "Sending FIN" << endl;
    if (sendto(s, &fin_pkt, sizeof(packet), 0, (struct sockaddr *) &si_other, slen) == -1){
        diep("sendto()");
    }
    while (true){
        packet ack;
        if (recvfrom(s, &ack, sizeof(packet), 0, (struct sockaddr *) &si_other, (socklen_t *) &slen) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK){
                diep("recvfrom()");
            }
            else{
                if (sendto(s, &fin_pkt, sizeof(packet), 0, (struct sockaddr *) &si_other, slen) == -1){
                    diep("sendto()");
                }
            }
        }
        else{
            if (ack.pkt_type == FINACK){
                cout << "Received FINACK" << endl;
                fin_pkt.pkt_type = FINACK;
                if (sendto(s, &fin_pkt, sizeof(packet), 0, (struct sockaddr *) &si_other, slen) == -1){
                    diep("sendto()");
                }
                cout << "DONE" << endl;
                break;
            }
        }
    }
        
}

/**
 * @brief Main sender function
 * 
 * @param hostname 
 * @param hostUDPport 
 * @param filename 
 * @param bytesToTransfer 
 */
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    /* Init */
    char temp_buf[sizeof(packet)];
    bytesToSend = bytesToTransfer;

    /* Open the file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

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

    /* Set timeout for the socket */
    struct timeval RTO;
    RTO.tv_sec = 0;
    RTO.tv_usec = TIMEOUT;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &RTO, sizeof(RTO)) == -1){
        diep("setsockopt()");
    }


	/* Send data and receive acknowledgements on s*/
    while(!dqueue.empty() || !aqueue.empty()){
        memset(temp_buf, 0, sizeof(packet));
        if (recvfrom(s, temp_buf, sizeof(packet), 0, (struct sockaddr *) &si_other, (socklen_t *) &slen) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK){
                diep("recvfrom()");
            }
            new_ack = false;
        }
        else{
            packet ack;
            memcpy(&ack, temp_buf, sizeof(packet));
            if (ack.pkt_type == ACK){
                if (ack.ack_idx == aqueue.front().seq_idx){
                    aqueue.pop();
                    new_ack = true;
                }
                else if (ack.ack_idx > aqueue.front().seq_idx){
                    while (!aqueue.empty() && ack.ack_idx >= aqueue.front().seq_idx){
                        aqueue.pop();
                    }
                    new_ack = true;
                }
                else{
                    new_ack = false;
                }
            }
        
        }
        handle_status();
    }
    end_connection();
    fclose(fp);

    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


