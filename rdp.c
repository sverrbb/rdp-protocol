#include "common.h"

/*****************************************************************************
------------------------------- RDP Protocol ---------------------------------
******************************************************************************

  Transport protocol RDP, residing on top of the “network protocol”. Provides
  multiplexing and reliability. The protocol use one single UDP port to receive
  packets from all clients, and it will answer them over this port as well.

******************************************************************************/




/*                      RDP CONNECTION FUNCTIONS                            */
/****************************************************************************/

/**
 * rdp connection function used by client for establishing connection
 * @param fd: socket used for sending connection
 * @param dest_addr: destination address of server
 * Function gives client a random id number
 * Makes an rdp packet and request connection by using flag 0x01
 * The function calls help method rdp_confirmation, waiting for final confirmation by server
 */
ssize_t rdp_connect(int fd, struct sockaddr_in dest_addr) {

    /* Generate random client id and make rdp connection packet*/
    int id = get_random_number();
    struct rdp_packet *pkt = make_rdp_packet(0x01, 0, 0, 0, id, 0, 0, NULL);

    /* Convert packet for sending */
    unsigned int size = sizeof(struct rdp_packet);
    char* convert = get_packet(pkt, &size);

    /* Send connection packet to server*/
    ssize_t wc = send_packet(fd, convert, size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    check_error(wc, "send_packet");

    /* Free allocated memory */
    free(pkt);
    free(convert);

    /* Wait for confirmation of established connection */
    ssize_t rc = rdp_confirmation(fd, &dest_addr);
    return rc;
}




/**
 * Rdp_confirmation function  used as help method in rdp_accept
 * Wait for confirmation by server and check that packet received is valid
 * @param fd: socket used for receiving packets from server
 * @param server_addr: address of server
 * If packet is received successfuly it will check that flag in packet
 * If flag == 20 than connection request has been declined
 * If flag == 0x10 than server has accepted connection request
 */
ssize_t rdp_confirmation(int fd, struct sockaddr_in *server_addr) {
    char buf[BUFSIZE];

    /* Set a timout to 1 second for receiving confirmation */
    struct timeval timeout = { 1, 0 };
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

    /* Try to receive packet from server */
    socklen_t addr_len = sizeof(struct sockaddr_in);
    ssize_t rc = recvfrom(fd, buf, BUFSIZE-1, 0 , (struct sockaddr*) server_addr, &addr_len);

    /* If there is no response from server within 1 second */
    if (rc < 0) {
        printf("No response from server. Please try again!\n");
        return rc;
    }

    /* Try to open packet an check that flag is valid
     * It will first check each bit in flag-byte and that there is a maximum of 1 digit equal 1 */
    struct rdp_packet *pkt = open_rdp_packet(buf, rc);
    int flag_check = check_bits_flag(&pkt -> flag, sizeof(char));
    if(flag_check == -1){
        printf("Received unavailable flag in rdp_connect(). Program exit!\n");
        free(pkt);
        return -1;
    }

    /* Check if connection request have been declined */
    if(pkt -> flag == 0x20) {
        printf("NOT CONNECTED: %d %d\n", pkt -> recvid,  pkt -> senderid);

        /* Use metadata to check the reason for rejecting connection request */
        /* If client id already exists */
        if(pkt -> metadata == 1){
            printf(" - Client-id is already connected to server\n");
        }

        /* If max files have been written*/
        else if (pkt -> metadata == 2){
            printf(" - Server has no more files to send\n");
        }

        /* Free packet and return -1 */
        free(pkt);
        return -1;
    }

    /* If connection request have been accepted packet contains flag 0x10 */
    if(pkt -> flag == 0x10) {
        printf("CONNECTED: %d %d\n", pkt -> senderid, pkt -> recvid);
        free(pkt);
        return rc;
    }

      /* If there is a valid flag but not an expected one */
    else {
        printf("Received unexpected packet in rdp_connect(). Exit program\n");
        free(pkt);
        return -1;
    }
}




/**
 * Rdp_listen function turn socket into a listening socket
 * Uses select funstion for listening
 * Uses a timout of 150ms to listen for new connection requests
 * Returns 1 if there is activity on socket
 * Returns 0 if time runs out
 */
int rdp_listen(int fd, fd_set fds, struct timeval timeout) {
    FD_ZERO(&fds);

    /* Set timeout to 100ms*/
    timeout.tv_sec = 0;
    timeout.tv_usec = 150000;

    FD_SET(fd, &fds);

    /* Listen for connection request */
    int rc = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
    check_error(rc, "select");

    /* If connection is received */
    if(FD_ISSET(fd, &fds)) {
        return 1;
    }

    /* If noe activity on socket*/
    return 0;
}




/**
 * @param fd: socket for receiving messages from clients
 * @param client_addr: pointer to client address
 * Check if packet is a connection request and establish connection
 * Uses rdp_send_accept as a help method for sending confirmation to client
 */
struct connection *rdp_accept(int fd, struct sockaddr_in *client_addr) {
    char buf[BUFSIZE];

    /* Receive packet from client */
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int rc = recvfrom(fd, buf, BUFSIZE -1, 0,(struct sockaddr*) client_addr, &addr_len);
    check_error(rc, "recvfrom");

    /* Open rdp_packet and store in struct */
    struct rdp_packet *pk = open_rdp_packet(buf, rc);

    /* Check if flag is valid */
    int flag_check = check_bits_flag(&pk -> flag, sizeof(char));
    if(flag_check == -1){
        printf("Received unavailable flag in rdp_accept(). Program exit!\n");
        free(pk);
        return NULL;
    }

    /* Check that id is unique and not already connected */
    int id_status = check_client_id(pk -> senderid);
    if(id_status == -1){
        ssize_t wc = rdp_send_reject(fd, *client_addr, pk -> senderid, 1);
        check_error(wc, "rdp_send_reject");
        free(pk);
        return NULL;
    }

    /* Check that not maximum number of files have been written */
    if(n_counter >= N){
        ssize_t wc = rdp_send_reject(fd, *client_addr, pk -> senderid, 2);
        check_error(wc, "rdp_send_reject");
        free(pk);
        return NULL;
    }

    /* Check that packet is a connection request - if so, flag == 0x01 */
    if(pk -> flag == 0x01) {
        struct connection *connection = rdp_send_accept(fd, *client_addr, pk -> senderid);
        n_counter++;
        free(pk);
        return connection;
    }

    /* Free packet and return 0 if flag not is a connection request */
    free(pk);
    return NULL;
}




/**
 * Function for sending accept packet to clients
 * Help method called by rdp_accept
 * The function makes an accept packet with flag 0x10 and send to client
 * It also calls function get_connection which returns a pointer to a connection
 */
struct connection *rdp_send_accept(int fd, struct sockaddr_in dest_addr, int id) {

    /* ID's for printing og connection to stdout */
    int client_id = id;
    int server_id = 0;

    /* Makes a rdp_packet with flag 0x10 which accept request from client */
    struct rdp_packet *pkt = make_rdp_packet(0x10, 0, 0, 0, client_id, 0, 0, NULL);
    unsigned int size = sizeof(struct rdp_packet);

    /* Convert packet for sending */
    char* convert = get_packet(pkt, &size);

    /* Send packet to client */
    ssize_t wc = send_packet(fd, convert, size, 0, (struct sockaddr*) &dest_addr, sizeof(dest_addr));
    check_error(wc, "send_packet");

    /* Create a connection pointer and return */
    struct connection *connection = get_connection(client_id, server_id, dest_addr);
    printf("CONNECTED %d %d\n", client_id, server_id);

    /* Free used packets */
    free(pkt);
    free(convert);

    /* Return established connection */
    return connection;
}




/**
 * Function rdp_send_reject for rejection connection request
 * Help method in rdp protocol called by rdp_accept
 * Uses flag 0x20 which tells client request has been rejected
 * @param id: id of client sending rejection
 * @param meta: number representing the reason for rejecting request
 */
ssize_t rdp_send_reject(int fd, struct sockaddr_in addr, int id, int meta) {
    ssize_t wc;
    int client_id = id;

    /* Make rdp_packet with flag 0x20 which reject connection request*/
    struct rdp_packet *pkt = make_rdp_packet(0x20, 0, 0, 0, 0, client_id, meta, NULL);

    /* Convert packet for sending */
    unsigned int size = sizeof(struct rdp_packet);
    char* convert = get_packet(pkt, &size);

    /* Send rejection packet to client */
    wc = send_packet(fd, convert, size, 0, (struct sockaddr*)&addr, sizeof(addr));
    check_error(wc, "send_packet");

    /* Free used packets */
    free(pkt);
    free(convert);

    /* Return write count from send_packet*/
    return wc;
}




/**
 * Creates a rdp_connection and returns it
 * @param client_id: unique id for each client
 * @param server_ id: always 0
 * @param client_addr: destination address for client
 * Sets variable file_status to 0 - used for multiplexing
 */
struct connection *get_connection(int client_id, int server_id, struct sockaddr_in client_addr) {

    /* Allocate memory for a connection */
    struct connection * cnt = malloc(sizeof(int) * 3 + sizeof(struct sockaddr_in));

    /* Check that connection was successfull */
    if (cnt == NULL) {
        fprintf(stderr, "malloc: could not allocate memory in get_connection()\n");
        free_all_rdp_connections();
        exit(EXIT_FAILURE);
    }

    /* Assign arguments to variables in struct */
    cnt -> client_id = client_id;
    cnt -> server_id = server_id;
    cnt -> file_status = 0;
    cnt -> client_addr = client_addr;

    /* Return connection */
    return cnt;
}




/**
 * Check that client id is unique and not already connected
 * Iterate through connections and check if connection is not NULL
 * If a connection with the same client_id already is connected - return -1
 * If client_id not is in connections - return 0
 */
int check_client_id(int client_id){
    for(int i = 0; i < N; i++){
        if(connections[i] != NULL){
            if(connections[i] -> client_id == client_id){
              return -1;
            }
        }
    }
    return 0;
}




/**
 * Initialize connection list
 * Allocate memory for n connection and
 * initialize each connection with NULL pointer
 * @param max_connections: max number of connections
 */
void init_connections(int max_connections) {
    n_counter = 0;
    max_addr = max_connections + 20;
    connections = malloc(sizeof(struct connection) * max_connections);
    for(int i = 0; i < max_connections; i++) {
        connections[i] = NULL;
    }
}




/**
 * Add connection to global list connections
 * Update index_counter for correct indexing in global list
 * @param connection: pointer to connection
 */
void add_rdp_connection(struct connection *connection) {
    for(int i = 0; i < N; i++) {
        if(connections[i] == NULL) {
          connections[i] = connection;
          return;
        }
    }
}


/****************************************************************************/










/*                     RDP FUNCTIONS FOR SENDING DATA                       */
/****************************************************************************/

/**
 * rdp_write function used for sending packet containing payload
 * Uses flag 0x04 for telling receiver that packet contain payload
 * @param sockfd: socket used for sending packet
 * @param buffer: buffer to read payload from
 * @param addr: destinations address for sending packet
 * @param retry: used for deciding pktseq in case of packet loss
 * @param len: size of payload to send
 */
ssize_t rdp_write(int sockfd, void *buffer, struct sockaddr_in addr, int retry, int len) {
    ssize_t wc;

    /* Get pktseq of packet. Check retry mode to account for packet loss */
    char pk = get_pktseq(retry);

    /* Make rdp_packet for sending */
    struct rdp_packet *pkt = make_rdp_packet(0x04, pk, 0, 0, 0, 0, len, buffer);

    /* Get size of rdp packet and convert it for sending */
    unsigned int size = sizeof(struct rdp_packet) + len;
    char* convert = get_packet(pkt, &size);

    /* Send rdp_packet to receiver */
    wc = send_packet(sockfd, convert, size, 0, (struct sockaddr*)&addr, sizeof(addr));

    /* Free used packets */
    free(pkt);
    free(convert);

    /* Return write count */
    return wc;
}




/**
 * rdp_EOF function used by server for marking end of file
 * Uses flag 0x20 for telling receiver that the whole file is sendt
 * @param fd: socket used for sending packet
 * @param addr: destinations address for sending packet
 * @param retry: used for deciding pktseq in case of packet loss
 */
ssize_t rdp_EOF(int fd, struct sockaddr_in addr, int retry) {
    ssize_t wc;

    /* Get pktseq of packet. Check retry mode to account for packet loss */
    char pk = get_pktseq(retry);

    /* Make rdp_packet for sending */
    struct rdp_packet *pkt = make_rdp_packet(0x20, pk, 0, 0, 0, 0, 0, NULL);

    /* Get size of rdp packet and convert it for sending */
    unsigned int size = sizeof(struct rdp_packet);
    char* convert = get_packet(pkt, &size);

    /* Send rdp_packet to receiver */
    wc = send_packet(fd, convert, size, 0, (struct sockaddr*)&addr, sizeof(addr));

    /* Free used packets */
    free(pkt);
    free(convert);

    /* Return write count */
    return wc;
}




/**
 * rdp_send_ack function used for sending packet containing ack
 * Uses flag 0x08 for telling receiver that packet contain ack
 * @param fd: socket used for sending packet
 * @param addr: destinations address for sending packet
 */
ssize_t rdp_send_ack(int fd, struct sockaddr_in addr, unsigned char ack) {
    ssize_t wc;

    /* Make rdp_packet for sending */
    struct rdp_packet *pkt = make_rdp_packet(0x08, 0, ack, 0, 0, 0, 0, NULL);

    /* Get size of rdp packet and convert it for sending */
    unsigned int size = sizeof(struct rdp_packet);
    char* convert = get_packet(pkt, &size);

    /* Send rdp_packet to receiver */
    wc = send_packet(fd, convert, size, 0, (struct sockaddr*)&addr, sizeof(addr));

    /* Free used packets */
    free(pkt);
    free(convert);

    /* Return write count */
    return wc;
}




/**
 * rdp_end_connection function used for sending packet containing connection ending
 * Uses flag 0x02 for telling receiver that packet contain connection ending
 * @param fd: socket used for sending packet
 * @param addr: destinations address for sending packet
 * @param retry: used for deciding pktseq in case of packet loss
 */
ssize_t rdp_end_connection(int fd, struct sockaddr_in addr, int retry) {
    ssize_t wc;

    /* Get pktseq of packet. Check retry mode to account for packet loss */
    char pk = get_pktseq(retry);

    /* Make rdp_packet for sending */
    struct rdp_packet *pkt = make_rdp_packet(0x02, pk, 0, 0, 0, 0, 0, NULL);

    /* Get size of rdp packet and convert it for sending */
    unsigned int size = sizeof(struct rdp_packet);
    char* convert = get_packet(pkt, &size);

    /* Send rdp_packet to receiver */
    wc = send_packet(fd, convert, size, 0, (struct sockaddr*)&addr, sizeof(addr));

    /* Free used packets */
    free(pkt);
    free(convert);

    /* Return write count */
    return wc;
}




/**
 * rdp_wait funkction which implements the "stop and wait" functionality
 * Uses a timpout of 100ms to check if any type of "confirmation" packet is received
 * The packets of interest are ack-packets (flag 0x08) and end-connection packets (flag 0x02)
 * @param sockfd: socket for for receiving packet
 */
ssize_t rdp_wait(int sockfd) {
    char r_buffer[BUFSIZE];

    /* 100ms timeout to wait for packet */
    struct timeval timeout = { 0, 100000 };
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

    /* Try to receive packet */
    ssize_t rc = recv(sockfd, r_buffer, BUFSIZE, 0);
    if (rc < 0) {
        return 0;
    }

    /* Open rdp_packet and store in struct */
    struct rdp_packet *pkt = open_rdp_packet(r_buffer, rc);

    /* Check if flag is valid */
    int flag_check = check_bits_flag(&pkt -> flag, sizeof(char));
    if(flag_check == -1){
        printf("Received unavailable flag in rdp_wait(). Program exit!\n");
        free(pkt);
        return -1;
    }

    // Return 1 if packet is an ack packet
    if(pkt -> flag == 0x08){
        unsigned char to_check = get_pktseq(1);

        if(pkt -> ackseq == to_check){
            free(pkt);
            return 1;
        }

        else {
            printf("Error: received ack packet, but not the correct ack-number!\n");
            free(pkt);
            return -1;
        }
    }

    // Return 1 if packet is a connection ending
    if(pkt -> flag == 0x02){
        free(pkt);
        return 1;
    }

    /* Return -1 if uncorrect flag is received*/
    free(pkt);
    return -1;
}




/**
 * Rdp_read function used for reading data packets in rdp protocol
 *
 * 1. Receives packet and read content into local buffer
 * 2. It then reads from buffer and and opens packet inside function
 * 3. Check flags in packet, both for validation and for information about the packet
 * 4. Copy payload into application buffer, which is then ready to be written
 * 5. Send ack back to server, confirming packet is received
 * 7. Return metadata, to be able to get size of payload in application
 *
 * @param sockfd: socket used for receiving and sending packets
 * @param buf: pointer to buffer to read from and write back to
 * @param size: size of buffer to know how much to read
 * @param addr: destinations address for sending ack
 * @param seq: sequence number to be acked and sendt back to server
 */
ssize_t rdp_read(int sockfd, char* buf, int size, struct sockaddr_in addr, unsigned char *seq){
    char buffer[size];
    ssize_t wc, rc = 0;
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);


    /* Use select to check if there is activity on socket
     * The function had in principle not needed to implement select as recv-
     * is a blocking call, but it turned out to get rid of a bug that sometimes
     * occurred when recv was used alone */
    int res = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
    check_error(res, "select");

    /* If activity on socket */
    if(FD_ISSET(sockfd, &fds)) {

        /* Try to receive packet */
        rc = recv(sockfd, buffer, size, 0);
        if(rc == -1){
            return rc;
        }
    }

    /* Open rdp_packet and store in struct */
    struct rdp_packet *new = open_rdp_packet(buffer, rc);
    // print_rdp_packet(new);

    /* Check if flag is valid */
    int flag_check = check_bits_flag(&new -> flag, sizeof(char));
    if(flag_check == -1) {
        printf("Received unavailable flag in rdp_read(). Program exit!\n");
        free(new);
        return -1;
    }

    /* If packet is an EOF packet  */
    if (new -> flag == 0x20) {
        wc = rdp_end_connection(sockfd, addr, 0);
        check_error(wc, "rdp_send_ack");
        free(new);
        return 0;
    }

    /* memcpy payload into application buffer */
    memcpy(buf, new -> payload, new -> metadata);
    *seq = new -> pktseq;
    int length = new -> metadata;

    /* Send ack back to server  */
    wc = rdp_send_ack(sockfd, addr, new -> pktseq);
    check_error(wc, "rdp_send_ack");

    /* Free used packet and return */
    free(new);
    return length;
}


/****************************************************************************/










/*                RDP FUNCTIONS FOR CLOSING CONNECTIONS                     */
/****************************************************************************/

/**
 * Close rdp connection
 * @param cnt: connection to close
 * Uses client_id to identify connection
 */
void rdp_close(struct connection *cnt) {
    remove_rdp_connection(cnt -> client_id);
}




/**
 * Remove connection between client and server
 * Gather global list and update N
 * @param client_id: unique id for identifiyng connection
 */
void remove_rdp_connection(int client_id) {
    for(int i = 0; i < N; i++) {
        if(connections[i] != NULL) {
            if(connections[i] -> client_id == client_id) {
                free_connection(connections[i]);
                connections[i] = NULL;
                printf("DISCONNECTED %d %d\n", client_id, 0 );
                return;
            }
        }
    }
    printf(" - Could not remove connection. No client_id with id:  %d\n", client_id);
}




/**
 * Free allocated memory for connection
 * @param connection: pointer to connection
 */
void free_connection(struct connection *connection) {
    free(connection);
}




/**
 * Free allocated memory for all connections
 * Uses the free_connection() function to free connections
 * Finally frees allocated memory for global list connections
 */
void free_all_rdp_connections() {
    for(int i = 0; i < N; i++) {
        if(connections[i] != NULL) {
            free_connection(connections[i]);
        }
    }
    free(connections);
}

/****************************************************************************/
