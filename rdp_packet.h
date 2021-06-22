#ifndef RDP_PACKET_H
#define RDP_PACKET_H

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

struct rdp_packet{
  unsigned char flag;
  unsigned char pktseq;
  unsigned char ackseq;
  unsigned char unnassigned;
  int senderid;
  int recvid;
  int metadata;
  char payload[0];
} __attribute__((packed));


char get_pktseq(int retry);

int get_random_number();

int check_bits_flag(void *flag, int size);

void print_rdp_packet(struct rdp_packet *pkt);

char *get_packet(struct rdp_packet *pkt, unsigned int *size);

struct rdp_packet* open_rdp_packet(char *d, unsigned int size);

struct rdp_packet *make_rdp_packet(unsigned char flag,
                                   unsigned char pktseq,
                                   unsigned char ackseq,
                                   unsigned char unnassigned,
                                   int senderid,
                                   int recvid,
                                   int metadata,
                                   char *payload);








#endif
