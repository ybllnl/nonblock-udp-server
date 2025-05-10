#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <time.h>
#include "orderbook.h"
#include <random>
#include <pthread.h>


#define NON_BLOCKING // for editor convenience, remove after coding finished
//#undef NON_BLOCKING
#define SERVER_PORT 8080


inline void process_message(char* buffer){
    // TODO: process message
    return;
}

Exchange exchange;
pthread_mutex_t exchange_mutex = PTHREAD_MUTEX_INITIALIZER;

void* simulate_other_orders(void* arg){
    while(true){
        pthread_mutex_lock(&exchange_mutex);
        exchange.simulate_other_orders();
        pthread_mutex_unlock(&exchange_mutex);
        sleep(1);
    }
}

int main() {

    pthread_t simulate_thread;
    pthread_create(&simulate_thread, NULL, simulate_other_orders, NULL);
    pthread_detach(simulate_thread);
    printf("Simulate thread created\n");

    struct timespec start, end;

    int socket_fd;
    char buffer[RECV_BUFFER_SIZE];

    // create socket
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(1);
    }


    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

#ifdef NON_BLOCKING
    set_non_blocking(socket_fd);
#endif

    if(bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Server is listening on port %d\n", ntohs(server_addr.sin_port));

#ifdef NON_BLOCKING
    printf("Using non-blocking server\n");
    // create epoll
    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0) {
        perror("epoll_create1");
        exit(1);
    }

    struct epoll_event event;
    // use edge-triggered: triggered when not ready -> ready
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = socket_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);

    struct epoll_event events[MAX_EVENTS];

    printf("Server enters the main loop\n");
    while(true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        for(int i = 0; i < num_events; i++) {
            int current_socket_fd = events[i].data.fd;
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            while(true){
                int recv_len = recvfrom(current_socket_fd, buffer, RECV_BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
                if(recv_len > 0) {
                    printf("Received message from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    printf("Message: %s\n", buffer);
                    process_message(buffer);
                    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
                    printf("Time taken: %ld microseconds\n", (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000);
                    break;

                }else if(recv_len == -1){
                    if(errno == EAGAIN || errno == EWOULDBLOCK) {
                        // no data to read
                        continue;
                    }else{
                        perror("recvfrom");
                        close(socket_fd);
                        close(epoll_fd);
                        exit(1);
                    }
                }else{
                    // zero bytes reads, ignore (udp should not happen)
                }
            }

        }
    }
#else
    printf("Using blocking server\n");
    while(true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int recv_len = recvfrom(socket_fd, buffer, RECV_BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if(recv_len > 0) {
            printf("Received message from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            printf("Message: %s\n", buffer);
            process_message(buffer);
        }else if(recv_len == -1){
            perror("recvfrom");
            close(socket_fd);
            exit(1);
        }else{
            // zero bytes reads, ignore (udp should not happen)
        }
    }
#endif
    return 0;
}