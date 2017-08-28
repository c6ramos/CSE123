#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

#define SEQUENCEMAX 1
#define PORT 61003
#define MAXDATASIZE 512

int main(void)
{
    
    char fileName[64];
    
    struct sockaddr_in serverAddress;
    struct hostent *hp;
    int socketNumber, x, recvlen;
    socklen_t addrLength = sizeof(serverAddress);
    char messageBuffer[2048];
    /* Test buffer data */
    messageBuffer[0] = 'H';
    messageBuffer[1] = '1';
    messageBuffer[2] = 'l';
    messageBuffer[3] = 'l';
    messageBuffer[4] = '\0';
    
    /* Create Socket */
    socketNumber = socket(AF_INET, SOCK_DGRAM, 0);
    
    /* Socket Error Check */
    if(socketNumber < 0){
        perror("Client: Cannot create socket");
        exit(0);
    }
    /* Fill in the server's sockaddr struct */
    memset((char *) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    
    /* Get server address (Need to check validity)*/
    hp = gethostbyname("localhost");
    
    /* Fill server sockaddr struct */
    memcpy((void *)&serverAddress.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    /* Now can send request */
    x = sendto(socketNumber, messageBuffer, strlen(messageBuffer), 0, (struct sockaddr*)&serverAddress, addrLength);
    printf("Message sent to server\n");
    
    
    
    for(;;){
        recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr*)&serverAddress, &addrLength);
        if(recvlen > 0){
            messageBuffer[recvlen] = '\0';
            printf("Message from server: %s\n", messageBuffer);
            
        }
    }
    
    return 0;
}

/**
 * Checks the filename for forbidden characters that may be used for a path.
 * @param message Filename to check
 * @return 0 if OK, 1 if message contains forbidden chars.
 */
int filenameCheck(char message[]){
    char forbiddenChars[11] = {'/', '\\', ':', '*', '\?','\"','<','>','|','~','\0'};

    for (int i = 0; i < strlen(forbiddenChars); i++)
        if (strchr(message, forbiddenChars[i]) != NULL){
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
 * @return 1 for RRQ, 2 for WRQ, 3 for DATA, 4 for ACK, 5 for ERROR
 */
char getOpcode(char message[]){
    return message[1];
}