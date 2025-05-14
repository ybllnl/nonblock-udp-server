#pragma once
#include <stdint.h>
#include <map>
#include <list>
#include <algorithm>
#include <random>
#include <queue>
#include "network_util.h"
#include "exchange_ip.h"
#include <unordered_set>

#define SEND_BUFFER_SIZE 1472
#define RECV_BUFFER_SIZE 65507

#define MULTICAST_IP_PRIMARY "224.0.0.1"
#define MULTICAST_IP_BACKUP "224.0.0.2"
#define MULTICAST_PORT_PRIMARY_SENDER 12344
#define MULTICAST_PORT_PRIMARY_RECEIVER 12345
#define MULTICAST_PORT_BACKUP_SENDER 12346
#define MULTICAST_PORT_BACKUP_RECEIVER 12347

const char* get_exchange_ip(){
    const char* env_ip = getenv("EXCHANGE_IP");
    return env_ip ? env_ip : DEFAULT_EXCHANGE_IP;
}

const uint8_t STOCK_NUM = 4;

uint64_t htonll(uint64_t value){
    return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | (htonl(value >> 32));
}

uint64_t ntohll(uint64_t value){
    return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | (ntohl(value >> 32));
}
typedef struct __attribute__((packed)) {
    // in total 232 bits = 29 bytes
    uint16_t Length;
    uint8_t  MessageType;
    uint64_t TimeOffset;
    uint64_t OrderID;
    char     SideIndicator;
    uint32_t Quantity;
    uint8_t  StockID;
    uint64_t Price;
    void print(){
        printf("AddOrderLongMessage: Length: %u, MessageType: %u, TimeOffset: %lu, OrderID: %lu, SideIndicator: %c, Quantity: %u, StockID: %u, Price: %lu\n", Length, MessageType, TimeOffset, OrderID, SideIndicator, Quantity, StockID, Price);
    }

    void serialize(){
        Length = htons(Length);
        TimeOffset = htonll(TimeOffset);
        OrderID = htonll(OrderID);
        Quantity = htonl(Quantity);
        Price = htonll(Price);
    }

    void deserialize(){
        Length = ntohs(Length);
        TimeOffset = ntohll(TimeOffset);
        OrderID = ntohll(OrderID);
        Quantity = ntohl(Quantity);
        Price = ntohll(Price);
    }

} AddOrderLongMessage;

typedef struct __attribute__((packed)) {
    uint16_t Length;
    uint8_t  MessageType;
    uint64_t TimeOffset;
    uint64_t OrderID;
    uint8_t StockID;
    void print(){
        printf("CancelOrderLongMessage: Length: %u, MessageType: %u, TimeOffset: %lu, OrderID: %lu, StockID: %u\n", Length, MessageType, TimeOffset, OrderID, StockID);
    }

    void serialize(){
        Length = htons(Length);
        TimeOffset = htonll(TimeOffset);
        OrderID = htonll(OrderID);
    }

    void deserialize(){
        Length = ntohs(Length);
        TimeOffset = ntohll(TimeOffset);
        OrderID = ntohll(OrderID);
    }
} CancelOrderLongMessage;


struct Order{
    uint8_t stock_id;
    enum Side {Bid, Ask};
    Side side;
    double price;
    uint32_t quantity;
    uint32_t order_id;

    bool operator<(const Order& other) const {
        if(price != other.price){
            return price < other.price;
        }
        return order_id < other.order_id;
    }

    bool operator==(const Order& other) const {
        return order_id == other.order_id;
    }
};

static inline uint64_t get_current_time_offset(){
    static struct timespec start_time;
    static bool initialized = false;
    struct timespec current_time;

    if(!initialized){
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
        initialized = true;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
    return (current_time.tv_sec - start_time.tv_sec) * 1000000000 + (current_time.tv_nsec - start_time.tv_nsec);
}

Order addmessage_to_order(const AddOrderLongMessage& message){
    Order order;
    order.stock_id = message.StockID;
    order.side = message.SideIndicator == 'B' ? Order::Side::Bid : Order::Side::Ask;
    order.price = message.Price;
    order.quantity = message.Quantity;
    order.order_id = message.OrderID;
    return order;
}

Order cancelmessage_to_order(const CancelOrderLongMessage& message){
    Order order;
    order.stock_id = message.StockID;
    order.order_id = message.OrderID;
    return order;
}


// OrderBook is a class that represents the orderbook of a stock
// exchange logic is not the focus of this project, just make a very simple one.
class alignas(64) OrderBook{
public:
    std::map<double, std::list<Order>> bids;
    std::map<double, std::list<Order>> asks;
    uint8_t stock_id;

    double current_price = -1;


    OrderBook() = default;
    ~OrderBook() = default;

    void add_order(const Order& order){
        if(order.stock_id >= STOCK_NUM){
            printf("Invalid stock id: %u\n", order.stock_id);
            return;
        }
        AddOrderLongMessage message = create_add_order_message(order);
        printf("Adding order: \n\t");
        message.print();
        auto& price_map = order.side == Order::Side::Bid ? bids : asks;

        if(price_map.find(order.price) == price_map.end()){
            price_map[order.price] = std::list<Order>();
        }
        price_map[order.price].push_back(order);

        match_order();
    }
    AddOrderLongMessage create_add_order_message(const Order& order){
        AddOrderLongMessage message;
        message.Length = sizeof(AddOrderLongMessage);
        message.MessageType = 1;
        message.TimeOffset = 0;
        message.OrderID = order.order_id;
        message.SideIndicator = order.side == Order::Side::Bid ? 'B' : 'S';
        message.Quantity = order.quantity;
        message.StockID = stock_id;
        message.Price = order.price;
        return message;
    }
    CancelOrderLongMessage create_cancel_order_message(const Order& order){
        CancelOrderLongMessage message;
        message.Length = sizeof(CancelOrderLongMessage);
        message.MessageType = 2;
        message.TimeOffset = 0;
        message.OrderID = order.order_id;
        message.StockID = stock_id;
        return message;
    }
    void cancel_order(const Order& order){
        printf("Cancelling order: %u, price: %.2f, quantity: %u, stock_id: %u\n", order.order_id, order.price, order.quantity, stock_id);
        auto& price_map = order.side == Order::Side::Bid ? bids : asks;
        auto it = price_map.find(order.price);
        if(it != price_map.end()){
            it->second.erase(std::find(it->second.begin(), it->second.end(), order));
        }
    }
    void match_order(){
        if(bids.empty() || asks.empty()){
            return;
        }

        while(true){
            auto bid = bids.rbegin();
            auto ask = asks.begin();

            if(bid == bids.rend() || ask == asks.end() || bid->first < ask->first){
                // no match
                break;
            }

            double price = (bid->first + ask->first) / 2;

            while(bid != bids.rend() && ask != asks.end() && bid->first >= ask->first){
                auto& bid_order = bid->second.front();
                auto& ask_order = ask->second.front();

                if(bid_order.price < ask_order.price){
                    break;
                }

                uint32_t trade_qunatity = std::min(bid_order.quantity, ask_order.quantity);
                printf("Trading %u shares at %.2f, stock_id: %u\n", trade_qunatity, price, stock_id);

                bid_order.quantity -= trade_qunatity;
                ask_order.quantity -= trade_qunatity;

                if(bid_order.quantity == 0){
                    bid->second.pop_front();
                }
                if(bid->second.empty()){
                    bids.erase(bid->first);
                    bid = bids.rbegin();
                }
                if(ask_order.quantity == 0){
                    ask->second.pop_front();
                }
                if(ask->second.empty()){
                    asks.erase(ask->first);
                    ask = asks.begin();
                }

            }

        }
    }

        
    
};



class ClientExchange{
public:
    OrderBook orderbooks[STOCK_NUM];
    std::unordered_set<uint32_t> processed_order_ids;

    struct sockaddr_in server_addr;

    void add_order(Order& order){
        if(order.stock_id >= STOCK_NUM){
            printf("Invalid stock id: %u\n", order.stock_id);
            return;
        }
        orderbooks[order.stock_id].add_order(order);
    }
    void cancel_order(const Order& order){
        orderbooks[order.stock_id].cancel_order(order);
    }

    ClientExchange(){
        for(int i = 0; i < STOCK_NUM; i++){
            orderbooks[i].stock_id = i;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        server_addr.sin_addr.s_addr = inet_addr(get_exchange_ip());
        printf("server_addr: %s:%d\n", get_exchange_ip(), SERVER_PORT);
    }

    void send_order_to_exchange(Order& order, int client_socket_fd){
        AddOrderLongMessage message = orderbooks[order.stock_id].create_add_order_message(order);
        message.TimeOffset = get_current_time_offset();
        printf("sending order: \n\t");
        message.print();
        message.serialize();
        while(true){
            int send_len = sendto(client_socket_fd, &message, sizeof(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
            if(send_len < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    continue;
                }
                perror("exchange send order");
                // Capture error details immediately
                int err = errno;
                char dest_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &server_addr.sin_addr, dest_ip, sizeof(dest_ip));
            
                fprintf(stderr, "Failed to send to %s:%d (socket %d). Error %d: %s\n",
                       dest_ip,
                       ntohs(server_addr.sin_port),
                       client_socket_fd,
                       err,
                       strerror(err));
            
                // Add specific checks
                if(err == EACCES) {
                    fprintf(stderr, "Possible causes:\n"
                                   "1. Multicast TTL not set (try setsockopt IP_MULTICAST_TTL)\n"
                                   "2. Firewall blocking UDP port %d\n"
                                   "3. Binding to privileged port without root\n",
                                   ntohs(server_addr.sin_port));
                }
                break;
            }else{
                printf("exchange send order success\n");
                break;
            }
        }
    }

};

class Exchange{
public:
    OrderBook orderbooks[STOCK_NUM];
    // keep it as simple as possible, only have add order and cancel order message queue
    std::queue<AddOrderLongMessage> add_order_message_queue;
    std::queue<CancelOrderLongMessage> cancel_order_message_queue;
    uint64_t next_order_id = 0;

    struct sockaddr_in multicast_primary_target_addr;
    struct sockaddr_in multicast_primary_sender_addr;
    struct sockaddr_in multicast_backup_target_addr;
    struct sockaddr_in multicast_backup_sender_addr;
    int primary_socket_fd;
    int backup_socket_fd;
    Exchange(){
        for(int i = 0; i < STOCK_NUM; i++){
            orderbooks[i].stock_id = i;
        }

        primary_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(primary_socket_fd < 0){
            perror("exchange multicast primary socket");
            exit(1);
        }
        backup_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(backup_socket_fd < 0){
            perror("exchange multicast backup socket");
            exit(1);
        }

        #ifdef TEST_LOCAL_NETWORK
        int ttl = 1;
        if(setsockopt(primary_socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0){
            perror("exchange multicast primary ttl");
            exit(1);
        }
        if(setsockopt(backup_socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0){
            perror("exchange multicast backup ttl");
            exit(1);
        }
        #endif

        memset(&multicast_primary_target_addr, 0, sizeof(multicast_primary_target_addr));
        multicast_primary_target_addr.sin_family = AF_INET;
        multicast_primary_target_addr.sin_port = htons(MULTICAST_PORT_PRIMARY_RECEIVER);
        multicast_primary_target_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP_PRIMARY);

        memset(&multicast_primary_sender_addr, 0, sizeof(multicast_primary_sender_addr));
        multicast_primary_sender_addr.sin_family = AF_INET;
        multicast_primary_sender_addr.sin_port = htons(MULTICAST_PORT_PRIMARY_SENDER);
        multicast_primary_sender_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        memset(&multicast_backup_target_addr, 0, sizeof(multicast_backup_target_addr));
        multicast_backup_target_addr.sin_family = AF_INET;
        multicast_backup_target_addr.sin_port = htons(MULTICAST_PORT_BACKUP_RECEIVER);
        multicast_backup_target_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP_BACKUP);

        memset(&multicast_backup_sender_addr, 0, sizeof(multicast_backup_sender_addr));
        multicast_backup_sender_addr.sin_family = AF_INET;
        multicast_backup_sender_addr.sin_port = htons(MULTICAST_PORT_BACKUP_SENDER);
        multicast_backup_sender_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        set_non_blocking(primary_socket_fd);
        set_non_blocking(backup_socket_fd);

        if(bind(primary_socket_fd, (struct sockaddr*)&multicast_primary_sender_addr, sizeof(multicast_primary_sender_addr)) < 0){
            perror("exchange multicast primary bind");
            exit(1);
        }
        if(bind(backup_socket_fd, (struct sockaddr*)&multicast_backup_sender_addr, sizeof(multicast_backup_sender_addr)) < 0){
            perror("exchange multicast backup bind");
            exit(1);
        }

    }

    void add_order(Order& order){
        order.order_id = next_order_id++;
        orderbooks[order.stock_id].add_order(order);
        add_order_message_queue.push(orderbooks[order.stock_id].create_add_order_message(order));
        send_multicast_messages();

    }
    void cancel_order(const Order& order){
        orderbooks[order.stock_id].cancel_order(order);
        cancel_order_message_queue.push(orderbooks[order.stock_id].create_cancel_order_message(order));
        send_multicast_messages();
    }


    void send_multicast_messages(){
        while(!add_order_message_queue.empty()){
            AddOrderLongMessage message = add_order_message_queue.front();
            message.TimeOffset = get_current_time_offset();
            printf("\tsending message: \n\t\t");
            message.print();
            message.serialize();
            while(true){
                ssize_t send_len = sendto(primary_socket_fd, &message, sizeof(message), 0, (struct sockaddr*)&multicast_primary_target_addr, sizeof(multicast_primary_target_addr));
                if(send_len < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        continue;
                    }
                    perror("exchange send multicast primary");
                    break;
                }else{
                    printf("exchange send multicast primary success\n");
                    printf("\tmessage:\n\t\t");
                    message.deserialize();
                    message.TimeOffset = get_current_time_offset();
                    message.print();
                    message.serialize();
                    break;
                }
            }
            while(true){
                ssize_t send_len = sendto(backup_socket_fd, &message, sizeof(message), 0, (struct sockaddr*)&multicast_backup_target_addr, sizeof(multicast_backup_target_addr));
                if(send_len < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        continue;
                    }
                    perror("exchange send multicast backup");
                    break;
                }else{
                    printf("exchange send multicast backup success\n");
                    printf("\tmessage:\n\t\t");
                    message.deserialize();
                    message.print();
                    break;
                }
            }
            add_order_message_queue.pop();
        }
        


        while(!cancel_order_message_queue.empty()){
            auto& message = cancel_order_message_queue.front();
            message.TimeOffset = get_current_time_offset();
            message.serialize();
            while(true){
                ssize_t send_len = sendto(backup_socket_fd, &message, sizeof(message), 0, (struct sockaddr*)&multicast_backup_target_addr, sizeof(multicast_backup_target_addr));
                if(send_len < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        continue;
                    }
                    perror("exchange send multicast backup");
                    break;
                }else{
                    printf("exchange send multicast backup success\n");
                    printf("\tmessage:\n\t\t");
                    message.deserialize();
                    message.TimeOffset = get_current_time_offset();
                    message.print();
                    message.serialize();
                    break;
                }
            }
            while(true){
                ssize_t send_len = sendto(primary_socket_fd, &message, sizeof(message), 0, (struct sockaddr*)&multicast_primary_target_addr, sizeof(multicast_primary_target_addr));
                if(send_len < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        continue;
                    }
                    perror("exchange send multicast primary");
                    break;
                }else{
                    printf("exchange send multicast primary success\n");
                    printf("\tmessage:\n\t\t");
                    message.deserialize();
                    message.print();
                    break;
                }
            }
            cancel_order_message_queue.pop();
        }

    }

    void simulate_other_orders(){
        // randomly choose a stock and a side
        // randomly choose a price between 1 and 100
        // randomly choose a quantity between 1 and 100
        // add the order

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, STOCK_NUM - 1);
        std::uniform_int_distribution<> dis_side(0, 1);
        std::uniform_int_distribution<> dis_price(1, 100);
        std::uniform_int_distribution<> dis_quantity(1, 100);

        int stock_id = dis(gen);
        int side = dis_side(gen);
        int price = dis_price(gen);
        int quantity = dis_quantity(gen);

        Order order;
        order.stock_id = stock_id;
        order.side = side == 0 ? Order::Side::Bid : Order::Side::Ask;
        order.price = price;
        order.quantity = quantity;

        add_order(order);
    }
        
};

