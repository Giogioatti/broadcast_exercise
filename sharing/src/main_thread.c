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
#include <pthread.h>



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

typedef struct Thread_args {
    int id;
    int sockfd;
    struct sockaddr_in broadcast_address;
    int *message_list;
    int *received;
    int num_received;
    fd_set readfd;
} thread_args;


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

void iniziatize_packet(Message * packet, int id){
    srand(time(NULL) + id); 
    packet->payload = rand();
    packet->source = id;
}

void send_packet(int sockfd, Message * packet, struct sockaddr_in * broadcast_address, socklen_t addr_len){
    if (sendto(sockfd, packet, sizeof(*packet), 0, (struct sockaddr *)broadcast_address, addr_len) < 0) {
            perror("sendto failed");
            exit(EXIT_FAILURE);
    }
}

void print_throughput(int * message_list, int id){
    printf("Throughput node %d: ", id);
    for ( int i = 0; i < MAX_NODES; i++){
        printf("%d: %03d, ", i, message_list[i]);
    }
    printf("\n");
}

void * broadcast_function(void * arg){

    thread_args *args = (thread_args *)arg;

    int sockfd = args->sockfd;
    int id = args->id;
    struct sockaddr_in broadcast_address = args->broadcast_address;

    Message packet;
    
    struct timeval start, now;

    // Starting time
    gettimeofday(&start, NULL);

    while(1){

        // Wait 10 ms
        usleep(10000);

        // Inizialize packet
        iniziatize_packet(&packet, id);

        // Assumption initial sequence number
        packet.sequence_number = 0;
        send_packet(sockfd, &packet, &broadcast_address, sizeof(broadcast_address));
        packet.sequence_number++;

        

        gettimeofday(&now, NULL);

        if (now.tv_sec - start.tv_sec >= DURATION)
            break;

    }
    return NULL;
}

void * receive_and_forward_function(void * arg){

    thread_args *args = (thread_args *)arg;

    fd_set readfd = args->readfd;
    int sockfd = args->sockfd;
    int id = args->id;
    struct sockaddr_in broadcast_address = args->broadcast_address;
    int * message_list = args->message_list;
    int * received = args->received;
    int num_received = args->num_received;

    struct sockaddr_in client_address;

    int addr_len = sizeof(client_address);

    struct timeval start, now, last;

    // Starting time
    gettimeofday(&start, NULL);

    last.tv_sec = 0;
    last.tv_usec = 0;

    while(1){
        FD_ZERO(&readfd);
        FD_SET(sockfd, &readfd);
        Message recvPacket;
            
        int ret = select(sockfd + 1, &readfd, NULL, NULL, 0);

        if (ret < 0){
            perror("select error");
            break;
        }

        else if (ret > 0){

            if (FD_ISSET(sockfd, &readfd)){

                if (recvfrom(sockfd, &recvPacket, sizeof(recvPacket), 0, (struct sockaddr *)&client_address, (socklen_t*)&addr_len) >= 0) {

                    // increments the counter of messages received by the node
                    message_list[recvPacket.source]++;

                    if (!is_received(recvPacket.payload, received, num_received)){

                        //printf("Node %d received value: %d, from: %d\n", id, recvPacket.payload,  recvPacket.source);

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

        if (now.tv_sec - start.tv_sec >= DURATION)
            break;

        if (now.tv_sec - last.tv_sec >= 1){
            print_throughput(message_list, id);
            last.tv_sec = now.tv_sec;
        }
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    int id = atoi(argv[1]);

    int sockfd;

    struct sockaddr_in broadcast_address;
    struct sockaddr_in client_address;
    struct sockaddr_in receiver_address;

    int yes = 1;
    fd_set readfd;

    int message_list[MAX_NODES];

    int * received = calloc(sizeof(int), MAX_MESSAGES);
    int num_received = 0;

    memset(message_list, 0, sizeof(message_list));


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


    pthread_t broadcast_thread, listen_thread;

    thread_args *t_args = malloc(sizeof(thread_args));

    // Popola la struttura con i dati necessari
    t_args->id = id;
    t_args->sockfd = sockfd;
    t_args->broadcast_address = broadcast_address;
    t_args->message_list = message_list;
    t_args->received = received;
    t_args->num_received = num_received;
    t_args->readfd = readfd;

    // Crea il thread di broadcast
    if(pthread_create(&broadcast_thread, NULL, broadcast_function, (void *) t_args)) {
        fprintf(stderr, "Errore nella creazione del thread di broadcast\n");
        return 1;
    }

    // Crea il thread di ascolto e inoltro
    if(pthread_create(&listen_thread, NULL, receive_and_forward_function, (void *) t_args)) {
        fprintf(stderr, "Errore nella creazione del thread di ascolto e inoltro\n");
        return 1;
    }

    // Aspetta la terminazione dei thread
    pthread_join(broadcast_thread, NULL);
    pthread_join(listen_thread, NULL);

    // Close file descriptors
    close(sockfd);
    // Free memory
    free(received);
}
