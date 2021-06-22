# RDP-protocol
The project is done as a Home Exam in the course IN2140 - Introduction to Operating Systems
and Data Communiation. The task was to create a new transport layer, RDP, the Reliable Datagram Protocol,
which provides multiplexing and reliability, a NewFSP client that retrieves files from
a server, and a NewFSP server that uses RDP to deliver large files to several NewFSP clients reliably.
The project assignment gives a further description of how the program is supposed to work.

## USER GUIDE
### COMPILE PROGRAM
 - make

### RUN PROGRAM
 - ./server <port> <filename> <number of files> <loss propability>
 - ./client <IP server> <port number> <loss propability>

### RUN PROGRAM WITH PRE-DEFINED VALUES
 - make run_server
 - make run_client

### CHECK PROGARAM USING VALGRIND WITH PRE-DEFINED VALUES
 - make valgrind_server
 - make valgrind_client


## IMPLEMENTATION:

### CONNECTION
For the program to work, the server application must be the first to start.
It will start by assigning the input values to variables, initializing a
list of rdp connections, creating a socket and binding its own address to
the socket. Then it will wait for incoming connect requests using the
rdp_listen function. The next thing that happens is that a client tries
to connect. This is done through the rdp_connect function called from the
client. The rdp_listen function will then notice that there is activity on the
socket and then call rdp_accept. The function then tries to add an rdp connection,
and if successful it sends a confirmation back to the client. Rdp_accept uses
the help functions rdp_send_accept and rdp_send_reject to respond to clients.
Rdp_connect in client then uses the help-function rdp_confirmation to receive
confirmation of established connection.


### MULTIPLEXING AND CONNECTION ENDING
For the multiplexing to work, the server first calls the get_total_file_packets
function to find out how many data packets each client should receive before the
whole file is sent. In the connections, which are added through rdp_accept and
add_rdp_connection, there is information about the file status of each client,
i.e how many packets they have received. Each time a packet is sent, the
file_status variable is updated. The program will go through one connection at
a time and try to deliver a packet before going to the next connected client.
When the maximum number of packets is sent, the server sends and EOF packet
to tell the client that the whole file has been sent. It will then wait for a
connection ending packet from the client, and then remove the temporary stored
connection and free allocated memory. The multiplexing and connection functionality
is implemented in an event loop, which first listens for new connection requests
and accepts, then it tries to deliver the next data packet to all connected clients,
and finally it check if a connection is to be ended.


### PACKET LOSS
To handle packet loss, the sequence number in the packets is used to identify
unique packets. Each time a new data packet is sent, the sequence number is
updated. If the server does not receive ack from the client, it will simply
send the same packet, with the same sequence number one more time. Rdp_read
in the client handles this by storing the sequence number of each received packet,
and checking that the next packet that arrives is updated from the previous one.
If it is not, the payload will not be written to file, and it will send ack back to
server and wait for next data packet.
