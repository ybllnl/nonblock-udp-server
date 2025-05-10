#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "network_util.h"
#include <errno.h>

#define NON_BLOCKING // for editor convenience, remove after coding finished
//#undef NON_BLOCKING

inline void process_message(char* buffer){
    // TODO: process message
    return;
}

int main() {
    int socket_fd;
    char buffer[RECV_BUFFER_SIZE];

    // create socket
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0); // 0 means ephemeral port
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

#ifdef NON_BLOCKING
    set_non_blocking(socket_fd);
#endif



    if(bind(socket_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Client is listening on port %d\n", ntohs(client_addr.sin_port));

    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    const char* message = "Hello, server!";
    strcpy(buffer, message);

    sendto(socket_fd, buffer, strlen(message), 0, (struct sockaddr*)&server_addr, server_addr_len);
#ifdef NON_BLOCKING

    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1){
        perror("epoll_create1");
        exit(1);
    }

    add_socket_to_epoll(epoll_fd, socket_fd);
    struct epoll_event events[MAX_EVENTS];

    printf("Using non-blocking client\n");
    while(true){
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for(int i = 0; i < num_events; i++){
            if(events[i].data.fd == socket_fd){
                printf("Data is ready to be read\n");
                int recv_len = recvfrom(socket_fd, buffer, RECV_BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &server_addr_len);
                if(recv_len > 0){
                    printf("Received %d bytes from server\n", recv_len);
                    printf("Message: %s\n", buffer);
                    process_message(buffer);
                }else if(recv_len == 0){
                    // UDP should not happen
                }else{
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        printf("No data to read\n");
                    }else{
                        perror("recvfrom");
                        exit(1);
                    }
                }
            }
        }
    }
#else
    printf("Using blocking client\n");
    while(true){

    }
#endif
    
    close(socket_fd);
    return 0;
}