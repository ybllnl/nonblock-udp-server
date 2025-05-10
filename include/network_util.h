#pragma once
#include <fcntl.h>
#include <sys/epoll.h>



#define SEND_BUFFER_SIZE 1472
#define RECV_BUFFER_SIZE 65507
#define SERVER_PORT 8080
const int MAX_EVENTS = 32;

inline void set_non_blocking(int socket_fd){
    int flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
}

void add_socket_to_epoll(int epoll_fd, int socket_fd){
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = socket_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == -1){
        perror("epoll_ctl");
        exit(1);
    }
}