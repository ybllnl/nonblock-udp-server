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

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0); // 0 means ephemeral port
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");



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
    
    close(socket_fd);
    return 0;
}