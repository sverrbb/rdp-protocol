#include "common.h"

/*****************************************************************************
------------------------------- RDP PACKET -----------------------------------
******************************************************************************/

unsigned char pktseq = 0b0000;

/**
 * Makes rdp packet used by protocol for communication between client and server
 * @param flag: defining different types of packets
 * @param pktseq: sequence number of packet
 * @param ackseq: sequence number ACK-ed by packet
 * @param unnassigned: unused, that is, always 0
 * @param senderid: sender ́s connection ID in network byte order
 * @param recvid: receiver ́s connection ID in network byte order
 * @param metadata: integer value in network byte order whose interpretation depends on the value of flags
 * @param payload: the number of bytes indicated by the previous integer value, max 1000 bytes
 */
struct rdp_packet *make_rdp_packet( unsigned char flag,
                                    unsigned char pktseq,
                                    unsigned char ackseq,
                                    unsigned char unnassigned,
                                    int senderid,
                                    int recvid,
                                    int metadata,
                                    char *payload){
    /* Define packet pointer */
    struct rdp_packet *pkt;

    /* If payload is to large */
    if(metadata > 999) {
        metadata = 999;
        pkt = malloc(sizeof(struct rdp_packet) + metadata);
        printf("Packet can not contain more then 1000 bytes!\n");
    }

    /* If metadata is used for error messages */
    else if(flag == 0x20){
        pkt = malloc(sizeof(struct rdp_packet));
    }

    /* Allocate memory for normal payload packet */
    else {
        pkt = malloc(sizeof(struct rdp_packet) + metadata);
    }

    /* Error allocating */
    if (pkt == NULL) {
        fprintf(stderr, "malloc: could not allacoate memory in make_rdp_packet()\n");
        free_all_rdp_connections();
        exit(EXIT_FAILURE);
    }

    /* Assign values to variables in struct */
    pkt -> flag = flag;
    pkt -> pktseq = pktseq;
    pkt -> ackseq = ackseq;
    pkt -> unnassigned = unnassigned;
    pkt -> senderid = senderid;
    pkt -> recvid = recvid;
    pkt -> metadata = metadata;

    /* Allocate memory if payload in packet */
    if(payload != NULL) {
      memcpy(pkt -> payload, payload, metadata);
    }

    /* Return "filled" packet */
    return pkt;
}



/**
 * Function for converting rdp_packet for sending
 * @param pkt: rdp_packet to be converted
 * @param size: pointer for getting size of converted packet
 */
char *get_packet(struct rdp_packet *pkt, unsigned int *size) {
    int total_size;

    /* Get total size of packet */
    if(pkt -> flag == 0x04){
      total_size = sizeof(struct rdp_packet) + pkt -> metadata;
    }

    /* Packet without payload */
    else {
      total_size = sizeof(struct rdp_packet);
    }

    /* Allocate memory */
    struct rdp_packet *to_send = malloc(total_size);

    /* Error allocating */
    if (to_send == NULL) {
        fprintf(stderr, "malloc: could not allacoate memory in get_packet()\n");
        free_all_rdp_connections();
        exit(EXIT_FAILURE);
    }

    /* Copy memory, and convert integers to network byte order */
    memcpy(to_send, pkt, total_size);
    to_send -> senderid = htonl(to_send -> senderid);
    to_send -> recvid = htonl(to_send -> recvid);
    to_send -> metadata = htonl(to_send -> metadata);

    /* Dereference pointer, get total size, cast to char pointer */
    *size = total_size;
    return (char *)to_send;
}



/**
 * Open rdp_packet after sending
 * @param converted_packet: rdp packet to open
 * @param size: size of packet
 * Function convert integers in packet from network byte order to host byte order
 * Allocate memory for packet with size equal argument size and memcpy content in packet
 */
struct rdp_packet* open_rdp_packet(char *d, unsigned int size) {

    /* Allocate memory for rdp_packet */
    struct rdp_packet *pkt = malloc(size);

    /* Check successful memory allocation */
    if (pkt == NULL) {
        fprintf(stderr, "malloc: could not allacoate memory in open_rdp_packet()\n");
        free_all_rdp_connections();
        exit(EXIT_FAILURE);
    }

    /* Convert received rdp_packet and assign to struct */
    memcpy(pkt, d, size);
    pkt -> senderid = ntohl(pkt -> senderid);
    pkt -> recvid = ntohl(pkt -> recvid);
    pkt -> metadata = ntohl(pkt -> metadata);

    return pkt;
}



/**
 * Print values in variables in struct rdp_packet
 * Used for debugging of program to check for correct values
 * @param pkt: pointer to packet to print
 */
void print_rdp_packet(struct rdp_packet *pkt) {
    printf("%d\n", pkt -> flag);
    printf("%d\n", pkt -> pktseq);
    printf("%d\n", pkt -> ackseq);
    printf("%d\n", pkt -> unnassigned);
    printf("%d\n", pkt -> senderid);
    printf("%d\n", pkt -> recvid);
    printf("%d\n", pkt -> metadata);
    printf("%s\n", pkt -> payload);
}



/**
 * Random number generator used for creating client id's
 * Will generate a random number between 0 - 999 and return
 */
int get_random_number(){
  time_t t;
  srand((unsigned) time(&t));
  int random_number_1 = 0 + rand() % 999;
  return random_number_1;
}



/**
 * Function for generating packet sequence number
 * Update number for each packet generated by rdp_write()
 * Update sequence number by 1 when called
 * @param retry: retry mode - no update of pktseq when trying to send previous packet
 */
char get_pktseq(int retry) {
    int seq = (int) pktseq;

    if(retry == 1){
        char new_seq = (unsigned char) seq;
        return new_seq;
    }

    seq += 1;
    char new_seq = (unsigned char) seq;
    pktseq = new_seq;
    return new_seq;
}



/**
 * Check bits in flag
 * @param flag: flag to check
 * @param size: size of flag datatype
 * Return -1 if more then 1 bit in flag is 1
 * Return 0 if there only are 1 or less bits in flag with 1
 */
int check_bits_flag(void *flag, int size){
    int counter = 0;
    unsigned char *to_bits = (unsigned char *) flag;
    for(int x = size - 1; x >= 0; x--){
        for(int y = 7; y >= 0; y--){
            char bit = (to_bits[x] & (1 << y)) ? '1' : '0';
            if(bit == '1'){
                counter++;
            }
        }
    }

    if(counter > 1){
        return -1;
    }

    return 0;
}

/****************************************************************************/
