#include "common.h"

/******************************************************************************
------------------------------ NewFSP-server ----------------------------------
*******************************************************************************

The NewFSP server will use RDP to receive connect requests from NewFSP clients
and serve them. The NewFSP server is a single-threaded process that can serve
several NewFSP clients same time.

When a client has connected successfully, the NewFSP server will transfer a large
file to that client. When it has transmitted the entire file, it sends an empty
packet to indicate that the client transfer is complete. It sends the same large
file to all of these clients. It uses RDP’s multiplexing provided to achieve this,
but it means that it must be able to accept new connections from a NewFSP client
in between sending packets to other, already connected NewFSP clients.
______________________________________________________________________________
******************************************************************************/




/**
 * Function used for multiplexing
 * @param filename: name of file to send
 * @param sockfd: socket file descriptor
 * @param addr: adress to send file
 * @param file_index: index to which part of file to send
 */
int send_file_packet( const char *filename,
                      int sockfd,
                      struct sockaddr_in addr,
                      int file_index) {

    FILE *fp;
    char buffer[BUFSIZE];
    ssize_t rc, wc;
    int a;
    int file_counter = 0;
    int retry = 0;

    // Open file
    fp = fopen(filename, "rb");
    if (fp == NULL){
        perror("Error: could not read file");
        exit(EXIT_FAILURE);
    }

    // Find packet with file_index
    while((a = fread(buffer,1, BUFSIZE, fp))) {
        if(file_counter == file_index) {
            while(1) {

                // Send packet to client
                wc = rdp_write(sockfd, buffer, addr, retry, a);
                check_error(wc, "rdp_write");

                // Wait for ack
                rc = rdp_wait(sockfd);
                if(rc > 0)  {
                    break;
                }

                // Turn on retry mode in rdp_write
                retry = 1;
            }
        }
         bzero(buffer, BUFSIZE);
         file_counter++;
    }
    fclose(fp);
    return 1;
}



/**
 * Tell the client that the whole file has been sent
 * Wait for confirmation from client
 */
int file_EOF(int fd, struct sockaddr_in addr){
    ssize_t wc, rc;
    int retry = 0;

    while(1) {

        // Send empty packet which marks end of file
        wc = rdp_EOF(fd, addr, retry);
        check_error(wc, "rdp_EOF");

        // Wait for connection ending
        rc = rdp_wait(fd);
        if(rc > 0) {
            break;
        }

        // Turn on retry mode
        retry = 1;
    }
    return 1;
}



/**
 * Get number of packets to send for file
 * Requires the send_file_packet function to use the same buffersize
 * @param filename: name of file to check
 */
int get_total_file_packets(const char *filename) {

    FILE *fp;
    char buffer[BUFSIZE];
    int number_of_packets = 0;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error: could not read file");
        exit(EXIT_FAILURE);
    }
    int a;
    while((a = fread(buffer,1, BUFSIZE, fp))) {
        number_of_packets++;
        bzero(buffer, BUFSIZE);
    }

    fclose(fp);
    return number_of_packets;
}




/**
 * Main function for NewFSP-server
 * 1. Create socket and bind address to socket
 * 2. Check if there are any new connection request
 * 3. Accept new connection request
 * 4. Write file datagram to client
 * 5. Check if whole file is sent and close connection
 */
int main(int argc, char const *argv[]) {

    if(argc < 5) {
        printf("Usage: %s <port> <filename> <number of files> <loss propability>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Assign input values to variables
    int port = atoi(argv[1]);
    const char *filename = argv[2];
    N = atoi(argv[3]);
    float prob = atof(argv[4]);
    set_loss_probability(prob);
    init_connections(N);

    // Get number of packets to send
    int max_value = get_total_file_packets(filename);
    int files_written = 0;
    int addr_index = 0;

    fd_set fds = { 0 };
    struct timeval timeout = { 0 };

    // Create socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    check_error(fd, "socket");

    // Get address
    struct sockaddr_in my_addr;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    // Decleare enough addresses
    struct sockaddr_in clients[max_addr];
    for(int x = 0; x < max_addr; x++) {
        struct sockaddr_in ad = { 0 };
        clients[x] = ad;
    }

    // Bind address to socket
    int rc = bind(fd, (struct sockaddr*) &my_addr, sizeof(struct sockaddr_in));
    check_error(rc, "bind");


    while(1){

        // 1. LISTEN AND ACCEPT CONNECTIONS
        int listening = rdp_listen(fd, fds, timeout);
        if(listening) {
            struct connection *ctn = rdp_accept(fd, &clients[addr_index]);
            if(ctn != NULL) {
                addr_index++;
                add_rdp_connection(ctn);
            }
        }

        // 2. MULTIPLEXING
        for(int i = 0; i < N; i++) {
            if(connections[i] != NULL) {
                if(connections[i] -> file_status <= max_value) {
                    int ind = connections[i] -> file_status;
                    if(ind < max_value) {
                        int sendt = send_file_packet(filename, fd, connections[i] -> client_addr, ind);
                        if(sendt) {
                            connections[i] -> file_status++;
                        }
                    } else {
                        int end = file_EOF(fd, connections[i] -> client_addr);
                        if(end) {
                            connections[i] -> file_status++;
                        }
                    }


                    // 3. CLOSE CONNECTION
                    if(connections[i] -> file_status > max_value) {
                        rdp_close(connections[i]);
                        files_written++;

                        if(files_written == N) {
                            free_all_rdp_connections();
                            close(fd);
                            return EXIT_SUCCESS;
                        }
                    }
                }
            }
        }
    }
}
