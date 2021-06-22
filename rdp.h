#ifndef RDP_H
#define RDP_H

// Includes used in RDP protocol
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>


// Global variables used in RDP protocol
int N;
int max_addr;
int n_counter;


// Connection struct
struct connection{
  int server_id;
  int client_id;
  int file_status;
  struct sockaddr_in client_addr;
}__attribute__((packed));


// Connection list used to store client connections
struct connection **connections;


// Functions used in RDP protocol
struct connection *get_connection(int client_id, int server_id, struct sockaddr_in client_addr);

struct connection *rdp_send_accept(int fd, struct sockaddr_in dest_addr, int id);

struct connection *rdp_accept(int fd, struct sockaddr_in *client_addr);

ssize_t rdp_connect(int fd, struct sockaddr_in dest_addr);

int rdp_listen(int fd, fd_set fds, struct timeval timeout);

ssize_t rdp_send_reject(int fd, struct sockaddr_in addr, int id, int meta);

ssize_t rdp_confirmation(int fd, struct sockaddr_in *server_addr);

ssize_t rdp_wait(int sockfd);

ssize_t rdp_send_ack(int fd, struct sockaddr_in addr, unsigned char ack);

ssize_t rdp_end_connection(int fd, struct sockaddr_in addr, int retry);

ssize_t rdp_write(int sockfd, void *buffer, struct sockaddr_in addr, int retry, int len);

ssize_t rdp_read(int sockfd, char* buf, int size, struct sockaddr_in addr, unsigned char *seq);

ssize_t rdp_EOF(int fd, struct sockaddr_in addr, int retry);

void free_connection(struct connection *connection);

void init_connections(int n);

void free_all_rdp_connections();

void add_rdp_connection(struct connection *connection);

void remove_rdp_connection(int client_id);

void rdp_close(struct connection *cnt);

int check_client_id(int client_id);



#endif
