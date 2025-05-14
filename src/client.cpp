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
#include "orderbook.h"

#define NON_BLOCKING // for editor convenience, remove after coding finished
//#undef NON_BLOCKING

ClientExchange exchange = ClientExchange();
inline void process_message(char* buffer, int recv_len, int client_socket_fd){
    // simulate the exchange locally
    uint8_t stock_id = 0;
    if(recv_len == sizeof(AddOrderLongMessage)){
        AddOrderLongMessage message;
        memcpy(&message, buffer, sizeof(AddOrderLongMessage));
        message.deserialize();
        message.print();
        if(exchange.processed_order_ids.find(message.OrderID) != exchange.processed_order_ids.end()){
            return;
        }
        exchange.processed_order_ids.insert(message.OrderID);
        Order order = addmessage_to_order(message);
        exchange.add_order(order);
        stock_id = order.stock_id;
    }else if(recv_len == sizeof(CancelOrderLongMessage)){
        CancelOrderLongMessage message;
        memcpy(&message, buffer, sizeof(CancelOrderLongMessage));
        message.deserialize();
        message.print();
        if(exchange.processed_order_ids.find(message.OrderID) != exchange.processed_order_ids.end()){
            return;
        }
        exchange.processed_order_ids.insert(message.OrderID);
        Order order = cancelmessage_to_order(message);
        exchange.cancel_order(order);
        stock_id = order.stock_id;
    }
    exchange.orderbooks[stock_id].match_order();

    // make trade decisions, not the focus of this project, we use random numbers to simulate
    // we may not want to explode the exchange for this project, thus assuming 2 clients
    // probability to generate a trade should be less than 1/2, say 0.4
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    if(dis(gen) < 0.4){
        Order order;
        // stock id is the same as the one in the message
        order.stock_id = stock_id;
        // side is random
        order.side = dis(gen) < 0.5 ? Order::Side::Bid : Order::Side::Ask;
        order.order_id = 0;
        // price is random
        order.price = dis(gen) * 100;
        // quantity is random
        order.quantity = dis(gen) * 100;
        exchange.send_order_to_exchange(order, client_socket_fd);
    }
    return;
}


int main() {
    int socket_primary_fd;
    int socket_backup_fd;
    char primary_buffer[RECV_BUFFER_SIZE];
    char backup_buffer[RECV_BUFFER_SIZE];

    // create socket
    socket_primary_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_primary_fd < 0) {
        perror("socket");
        exit(1);
    }

    socket_backup_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_backup_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in primary_client_addr;
    memset(&primary_client_addr, 0, sizeof(primary_client_addr));
    primary_client_addr.sin_family = AF_INET;
    primary_client_addr.sin_port = htons(MULTICAST_PORT_PRIMARY_RECEIVER);
    primary_client_addr.sin_addr.s_addr = INADDR_ANY;

    struct sockaddr_in backup_client_addr;
    memset(&backup_client_addr, 0, sizeof(backup_client_addr));
    backup_client_addr.sin_family = AF_INET;
    backup_client_addr.sin_port = htons(MULTICAST_PORT_BACKUP_RECEIVER);
    backup_client_addr.sin_addr.s_addr = INADDR_ANY;

    // enable port and address reuse
    int reuse = 1;
    if(setsockopt(socket_primary_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0){
        perror("setsockopt reuse primary");
        exit(1);
    }
    if(setsockopt(socket_backup_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0){
        perror("setsockopt reuse backup");
        exit(1);
    }
#ifdef NON_BLOCKING
    set_non_blocking(socket_primary_fd);
    set_non_blocking(socket_backup_fd);
#endif



    if(bind(socket_primary_fd, (struct sockaddr*)&primary_client_addr, sizeof(primary_client_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if(bind(socket_backup_fd, (struct sockaddr*)&backup_client_addr, sizeof(backup_client_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    // join multicast group
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP_PRIMARY);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if(setsockopt(socket_primary_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("failed to join primary multicast group");
        exit(1);
    }

    ip_mreq mreq_backup;
    mreq_backup.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP_BACKUP);
    mreq_backup.imr_interface.s_addr = INADDR_ANY;
    if(setsockopt(socket_backup_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq_backup, sizeof(mreq_backup)) < 0) {
        perror("failed to join backup multicast group");
        exit(1);
    }

    printf("Client is listening on port %d\n", ntohs(primary_client_addr.sin_port));
    printf("Client is listening on port %d\n", ntohs(backup_client_addr.sin_port));

    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    const char* message = "Hello, server!";
    strcpy(primary_buffer, message);

    sendto(socket_primary_fd, primary_buffer, strlen(message), 0, (struct sockaddr*)&server_addr, server_addr_len);
#ifdef NON_BLOCKING

    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1){
        perror("epoll_create1");
        exit(1);
    }

    add_socket_to_epoll(epoll_fd, socket_primary_fd);
    add_socket_to_epoll(epoll_fd, socket_backup_fd);
    struct epoll_event events[MAX_EVENTS];

    printf("Using non-blocking client\n");
    while(true){
        printf("epoll_wait\n");
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for(int i = 0; i < num_events; i++){
            char* buffer;
            int socket_fd;
            if(events[i].data.fd == socket_primary_fd){
                buffer = primary_buffer;
                socket_fd = socket_primary_fd;
                printf("Primary socket received data\n");
            }else if(events[i].data.fd == socket_backup_fd){
                buffer = backup_buffer;
                socket_fd = socket_backup_fd;
                printf("Backup socket received data\n");
            }else{
                perror("epoll_wait unknown socket");
                exit(1);
            }
            printf("Data is ready to be read\n");
            int recv_len = recvfrom(socket_fd, buffer, RECV_BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &server_addr_len);
            if(recv_len > 0){
                process_message(buffer, recv_len, socket_fd);
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
#else
    printf("Using blocking client\n");
    while(true){

    }
#endif
    
    close(socket_primary_fd);
    close(socket_backup_fd);
    return 0;
}