#include "common.h"


/*******************************************************************************
--------------------------------- NewFSP-client --------------------------------
********************************************************************************

The NewFSP client will use RDP to connect to a NewFSP server and retrieve a file.
The client does not know the name of this file; it will simply establish an RDP
connection to the server and receive packets. The NewFSP client will store these
packets in a local file called kernel-file-<random number>. When the NewFSP server
indicates that the file transfer is complete by sending an empty packet, the NewFSP
client will close the connection.

********************************************************************************/



/**
 * Read file packets from server and write payload to file
 * Uses RDP_read() to read packet and then sends ack back to server
 * Account for packet loss through comparing received with last received packet
 * Finish when receiving EOF packet from server
 */
void read_and_write_file(int sockfd, char *filename, struct sockaddr_in addr){
    FILE *fp;
    ssize_t rc, wc = 0;
    char buffer[SIZE];
    unsigned char last = -1;
    unsigned char new;

    // Open file
    fp = fopen(filename, "wb");
    if (fp == NULL){
        perror("Error: could not read file");
        exit(EXIT_FAILURE);
    }

    // Loop until all packet are received
    while(1){

        // Try to read payload from server into buffer
        rc = rdp_read(sockfd, buffer, SIZE, addr, &new);
        check_error(rc, "rdp_read");

        // If rc == 0 rdp has received EOF packet and returns
        if(rc == 0){
            fclose(fp);
            return;
        }

        // Check that new packet is not the same as last
        if(new != last){

            wc = fwrite(buffer, 1, rc, fp);

            if(wc != rc){
              fprintf(stderr, "fwrite failed\n");
              exit(EXIT_FAILURE);
            }

            last = new;
        }

        // Empty buffer for new packet
        bzero(buffer, SIZE);
        wc = 0;
    }

    // Close file and return
    fclose(fp);
    return;
}





/**
 * Function for generating filname with random numbers
 * Generate three separate random numbers and add to
 * the last three indexes in input name. For the function to
 * work, param: 'name' has to account for the last three characters
 * two be replaced. E.g 'kernel-file-XXX' as input
 */
char *get_filename(char *name) {
    time_t t;
    srand((unsigned) time(&t));

    // Generate two random numbers between 0-9
    int random_number_1 = 0 + rand() % 9;
    int random_number_2 = 0 + rand() % 9;
    int random_number_3 = 0 + rand() % 9;

    // Get indexes for numbers
    int number_index_1 = strlen(name) - 3;
    int number_index_2 = strlen(name) - 2;
    int number_index_3 = strlen(name) - 1;

    // Duplicate, allocate string
    char *filename = strdup(name);

    // Add random numbers to filename
    filename[number_index_1] = random_number_1 + '0';
    filename[number_index_2] = random_number_2 + '0';
    filename[number_index_3] = random_number_3 + '0';

    // Returns filename
    return filename;
}



/**
 * Generating unique filename by checking if filename already exists
 * Uses access to checks whether the calling process can access the file pathname.
 * Uses get_filename to generate filename and then check in loop
 */
char *generate_unique_filename(){
    char* filename = get_filename("kernel-file-XXX");
    while(1){

        // If filename already exists in folder
        if (access(filename, F_OK) == 0) {
            free(filename);
            filename = get_filename("kernel-file-XXX");
        }

        // If filename is unique
        else {
            return filename;
        }
    }
}



/**
 * Main function for NewFSP client
 * 1. Create socket and get address of server
 * 2. Connect to server through rdp_connect
 * 3. Receive file from server and write to file
 */
int main(int argc, char const *argv[]) {

    // Check correct number of input arguments
    if(argc < 4) {
        printf("usage: %s <IP server> <port number> <loss propability>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Assign values to variables
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    float prob = atof(argv[3]);
    set_loss_probability(prob);

    // Create socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    check_error(fd, "socket");

    // Get ip address
    struct in_addr ip_addr;
    int wc = inet_pton(AF_INET, ip, &ip_addr.s_addr);
    check_error(wc, "inet_pton");
    if(!wc) {
        fprintf(stderr, "Invalid IP adress: %s\n", ip );
        return EXIT_FAILURE;
    }

    // Get server address
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr = ip_addr;

    // Try to connect to server
    ssize_t res = rdp_connect(fd, dest_addr);
    if(res == -1){
      return EXIT_SUCCESS;
    }

    // Generate filename with random number-ending
    char *filename = generate_unique_filename();

    // Read file packets and write to file using RDP protocol
    read_and_write_file(fd, filename, dest_addr);

    // Prints name of written file
    printf("%s\n", filename);
    free(filename);

    // Close socket and exit program
    close(fd);
    return EXIT_SUCCESS;
}
