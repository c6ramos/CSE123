#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>

#define SEQUENCEMAX 1
#define PORT 61003
#define MAXDATASIZE 512

int
main(int argc, char **argv)
{    
    FILE *myFile;
    int res;
    char fileName[64];
    
    struct sockaddr_in myAddress, clientAddress;
    int socketNumber, x, recvlen;
    char messageBuffer[2048];
    socklen_t addrLength = sizeof(clientAddress);
    
    /* Create Socket */
    socketNumber = socket(AF_INET, SOCK_DGRAM, 0);
    
    /* Socket Error Check */
    if(socketNumber < 0){
        perror("Server: Cannot create socket");
        exit(0);
    }
    
    /* Fill in the server's sockaddr struct */
    memset((char *) &myAddress, 0, sizeof(myAddress));
    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddress.sin_port = htons(PORT);
    
    /* Binding socket to address and port(Server Only) */
    /* NEED TO ERROR CHECK x */
    x = bind(socketNumber, (struct sockaddr *) &myAddress, sizeof(myAddress));
    
    /* Setup complete ready to receive data */
    for(;;){
        recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) &clientAddress, &addrLength);
        if(recvlen > 0){
            switch(getOpcode(messageBuffer)){
                case '1':
                    if (filenameCheck(fileName)==1){
                        //Filename contains forbidden chars
                        return 1;
                    }
                    sprintf(fileName, "server/%s.txt", (messageBuffer+2));
                    myFile = fopen(fileName, "r");
                    res = fread(messageBuffer+2, 1, 25, myFile);
                    break;
                case '2':
                    messageBuffer[1] = 'B';
                    break;
                case '3':
                    break;
            }
            
            messageBuffer[25] = '\0';
            //printf("Message from Client: %s", buf);
            sendto(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) &clientAddress, addrLength);
        }
    }
    
    return 0;
}

/**
 * Checks the filename for forbidden characters that may be used for a path.
 * @param str Filename to check
 * @return 0 if OK, 1 if str contains forbidden chars.
 */
int filenameCheck(char str[]){
    char forbiddenChars[11] = {'/', '\\', ':', '*', '\?','\"','<','>','|','~','\0'};

    for (int i = 0; i < strlen(forbiddenChars); i++)
        if (strchr(str, forbiddenChars[i]) != NULL){
            return 1;
        }
    return 0;
}

/**
 * Returns the next number in packet sequence. Returns 0 if at max sequence number
 * @param currentSequenceNum Current sequence number
 * @return 0 if sequence at max. Else, next number in sequence
 */
int nextSequenceNum(int currentSequenceNum){
    if (currentSequenceNum == int(SEQUENCEMAX)){
        return 0;
    }
    return currentSequenceNum+1;
}

/**
 * Gets the opcode from the message header.
 * @return 01 for RRQ, 02 for WRQ, 03 for DATA, 04 for ACK, 05 for ERROR
 */
int getOpcode(char message[]){
    return message[1];
}