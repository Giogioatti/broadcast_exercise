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

// Function to check if the sending node is a neighbor
bool is_neighbor(int value, int * neighbors, int size){
    for (int i = 0; i < size; i ++) {
        if (neighbors[i] == value)
            return true;
    }
    return false;
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


int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 3) {
        perror("No neighbor for this node");
        return 1;
    }

    int id = atoi(argv[1]);

    log_msg(id, "begin \n");

    int * neighbors = calloc(sizeof(int), argc - 2);
    for (int i = 0; i < argc - 2; i ++) {
        neighbors[i] = atoi(argv[i + 2]);
        printf("%d-", neighbors[i]);
    }

    int neighbors_len = argc - 2;
    int sockfd;
    struct sockaddr_in broadcast_address;
    struct sockaddr_in receiver_address;
    struct sockaddr_in client_address;
    Message packet;
    int yes = 1;
    int addr_len = sizeof(receiver_address);
    fd_set readfd;
    int * received = calloc(sizeof(int), MAX_MESSAGES);
    int num_received = 0;

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

    if (argc >= 3 && strcmp(argv[1], "0") == 0) {
        sleep(1); // Wait 1 second before starting the broadcast

        // Random number generator
        srand(time(NULL) + id); 
        packet.payload = rand();
        packet.source = id;

        // Assumption initial sequence number
        packet.sequence_number = 0;

        // Sending the packet in broadcast
        if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&broadcast_address, addr_len) < 0) {
            perror("sendto failed");
            exit(EXIT_FAILURE);
        }

        printf("Node %d (leader) broadcasting value: %d\n", id, packet.payload);
    }

    while (1) {
        FD_ZERO(&readfd);
        FD_SET(sockfd, &readfd);
        Message recvPacket;

        int ret = select(sockfd + 1, &readfd, NULL, NULL, 0);
        if (ret > 0)
        {
            if (FD_ISSET(sockfd, &readfd))
            {

                if (recvfrom(sockfd, &recvPacket, sizeof(recvPacket), 0, (struct sockaddr *)&client_address, (socklen_t*)&addr_len) >= 0) {
                    
                    // Check if the packet is already seen or sent by the node itself and if a neighbor sends it
                    if (recvPacket.payload != packet.payload && is_neighbor(recvPacket.source, neighbors, neighbors_len) && !is_received(recvPacket.payload, received, num_received)) {
                        printf("Node %d received value: %d, from: %d\n", id, recvPacket.payload,  recvPacket.source);

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
                else{
                    perror("recvfrom failed");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // Close file descriptors
    close(sockfd);

    // Free memory
    free(neighbors);
    free(received);
}
