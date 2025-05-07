#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SEND_BUFFER_SIZE 1472
#define RECV_BUFFER_SIZE 65507
#define SERVER_PORT 8080
int main() {
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

    if(bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Server is listening on port %d\n", ntohs(server_addr.sin_port));

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while(true) {
        int recv_len = recvfrom(socket_fd, buffer, RECV_BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if(recv_len < 0) {
            perror("recvfrom");
            exit(1);
        }

        printf("Received message from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("Message: %s\n", buffer);
    }
    
    close(socket_fd);
    return 0;
}