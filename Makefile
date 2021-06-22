#----------------------------------------
#					  	VARIABLES
#----------------------------------------
CC = gcc
CFLAGS = -std=gnu11 -g -Wall -Wextra
VFLAGS = --track-origins=yes --leak-check=full --show-leak-kinds=all --malloc-fill=0x40 --free-fill=0x23
OBJFILES1 = newFSP-client.o send_packet.o rdp.o common.o rdp_packet.o
OBJFILES2 = newFSP-server.o send_packet.o rdp.o common.o rdp_packet.o
HFILES = send_packet.h common.h rdp_packet.h rdp.h
RM = rm -rf
BIN = client server
PORT = 2628
CLIENTARGS = client 127.0.0.1 $(PORT) 0.06
SERVERARGS = server $(PORT) H1-Multiplexing-and-Loss-Recovery.pdf 4 0.06
#----------------------------------------




#----------------------------------------
#						FIRST TARGET
#----------------------------------------
all: $(BIN)
#----------------------------------------




#----------------------------------------
#						COMPILE PROGRAM
#----------------------------------------
# Compile client
client: $(OBJFILES1) $(HFILES)
	$(CC) $(CFLAGS) $(OBJFILES1) -o client

# Compile server
server: $(OBJFILES2) $(HFILES)
	$(CC) $(CFLAGS) $(OBJFILES2) -o server
#----------------------------------------



#----------------------------------------
#				CREATE OBJECT FILES
#----------------------------------------
# Creates object file for newFSP-client
newFSP-client.o: newFSP-client.c
	$(CC) $(CFLAGS) -c newFSP-client.c

# Creates object file for newFSP-server
newFSP-server.o: newFSP-server.c
	$(CC) $(CFLAGS) -c newFSP-server.c

# Creates object file for send_packet
send_packet.o: send_packet.c
	$(CC) $(CFLAGS) -c send_packet.c

# Creates object file for rdp
rdp.o: rdp.c
	$(CC) $(CFLAGS) -c rdp.c

# Creates object file for rdp_packet
rdp_packet.o: rdp_packet.c
	$(CC) $(CFLAGS) -c rdp_packet.c

# Creates object file for common
common.o: common.c
	$(CC) $(CFLAGS) -c common.c

#----------------------------------------



#----------------------------------------
#					  RUN PROGRAM
#----------------------------------------
# Run client application
run_client: client
	./$(CLIENTARGS)

# Run server application
run_server: server
	./$(SERVERARGS)
#----------------------------------------



#----------------------------------------
#				RUN PROGRAM WITH VALGRIND
#----------------------------------------
# Runs valgrind on client
valgrind_client: client
	valgrind $(VFLAGS) ./$(CLIENTARGS)

# Runs valgrind on server
valgrind_server: server
	valgrind $(VFLAGS) ./$(SERVERARGS)
#----------------------------------------



#----------------------------------------
#						REMOVE FILES
#----------------------------------------
# Remove executable, object- and program files
clean:
	$(RM) $(BIN) *.dSYM *.o kernel-file*
#----------------------------------------
