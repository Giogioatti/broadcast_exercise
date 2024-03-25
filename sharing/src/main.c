#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>


#define PORT 8080
#define MAX_MESSAGES 50
#define MAX_NODES 10
#define BROADCAST_INTERVAL 10000
#define DURATION 20

// Struct messagge
    typedef struct Message {
    int source;
    int sequence_number;
    int payload;
} Message;

void log_msg(int id, char * format, ...) {
    va_list args;
    va_start(args, format);
    printf("%d: ", id);
    printf(format, args);
    va_end(args);
}

// Function to check if a message has already been received
bool is_received(int value, int * received, int size){
    for ( int i = 0; i < size; i ++){
        if (received[i] == value)
            return true;
    }
    return false;
}

// Function to add a message to the list of received messages
void add_received(int value, int * received, int * size){
    received[*size] = value;
    (*size)++;
}

// Function to inizialize a packet
void iniziatize_packet(Message * packet, int id){
    srand(time(NULL) + id); 
    packet->payload = rand();
    packet->source = id;
}

// Function to send a packet in broadcast
void send_packet(int sockfd, Message * packet, struct sockaddr_in * broadcast_address, socklen_t addr_len){
    if (sendto(sockfd, packet, sizeof(*packet), 0, (struct sockaddr *)broadcast_address, addr_len) < 0) {
            perror("sendto failed");
            exit(EXIT_FAILURE);
    }
}

// Function to print throughput
void print_throughput(int * message_list, int id){
    printf("Throughput node %d: ", id);
    for ( int i = 0; i < MAX_NODES; i++){
        printf("%d: %03d, ", i, message_list[i]);
    }
    printf("\n");
}


int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int id = atoi(argv[1]);

    int sockfd;

    struct sockaddr_in broadcast_address;
    struct sockaddr_in receiver_address;
    struct sockaddr_in client_address;
    int addr_len = sizeof(receiver_address);

    Message packet;
    int yes = 1;

    // Array to count the messages arrived from the other nodes
    int message_list[MAX_NODES];

    // Array used to store arrived messages and to discard packets already seen
    int * received = calloc(sizeof(int), MAX_MESSAGES);
    int num_received = 0;

    memset(message_list, 0, sizeof(message_list));

    // time structures 
    struct timeval tv, start, now, last;

    fd_set readfd;

    // Creating the UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setting the socket to allow broadcasting
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&yes, sizeof(yes)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Receiving from any address
    memset(&receiver_address, 0, sizeof(receiver_address));
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_port = htons(PORT);
    receiver_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // Sending in broadcast
    memset(&broadcast_address, 0, sizeof(broadcast_address));
    broadcast_address.sin_family = AF_INET;
    broadcast_address.sin_port = htons(PORT);
    broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);


    // Associate all receiving addresses
    if (bind(sockfd, (struct sockaddr *)&receiver_address, sizeof(receiver_address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Starting time
    gettimeofday(&start, NULL);

    last.tv_sec = 0;
    last.tv_usec = 0;
    tv.tv_sec = 0;

        while(1) {

            tv.tv_usec = BROADCAST_INTERVAL;

            FD_ZERO(&readfd);
            FD_SET(sockfd, &readfd);
            Message recvPacket;
            
            int ret = select(sockfd + 1, &readfd, NULL, NULL, &tv);

            if (ret < 0){
                perror("select error");
                break;
            }

            // Timeout
            else if (ret == 0){
                // Inizialize packet
                iniziatize_packet(&packet, id);
                // Assumption initial sequence number
                packet.sequence_number = 0;
                send_packet(sockfd, &packet, &broadcast_address, sizeof(broadcast_address));
                packet.sequence_number++;
            }

            else if (ret > 0)
            {
                if (FD_ISSET(sockfd, &readfd))
                {

                    if (recvfrom(sockfd, &recvPacket, sizeof(recvPacket), 0, (struct sockaddr *)&client_address, (socklen_t*)&addr_len) >= 0) {

                        // increments the counter of messages received by the node
                        message_list[recvPacket.source]++;

                        if (!is_received(recvPacket.payload, received, num_received)){

                            // Adds to received packets
                            add_received(recvPacket.payload, received, &num_received);        

                            // Increment the sequence number and resend the packet
                            recvPacket.sequence_number++;
                            recvPacket.source = id;
                            if (sendto(sockfd, &recvPacket, sizeof(recvPacket), 0, (struct sockaddr *)&broadcast_address, addr_len) < 0) {
                                perror("sendto failed");
                                exit(EXIT_FAILURE);
                            }
                        }
                        
                    }
                }
            }

            gettimeofday(&now, NULL);

            // If it's been 20 seconds since you started, break
            if (now.tv_sec - start.tv_sec >= DURATION)
                break;

            // if 1 second has passed since the previous print, print throughput
            if (now.tv_sec - last.tv_sec >= 1){
                print_throughput(message_list, id);
                last.tv_sec = now.tv_sec;
            }
        }

    // Close file descriptors
    close(sockfd);
    // Free memory
    free(received);
}
