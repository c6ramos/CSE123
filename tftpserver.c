#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>

#define SEQUENCEMAX 1
#define PORT 61003
#define MAXDATASIZE 512
#define TIMEOUT 5
#define RETRYMAX 10

void wrqHandler(int socketNumber, char* messageBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile );

void rrqHandler(int socketNumber, char* receiveBuffer, char* sendBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ); 
//Send ACK function
void sendACK(int socketNumber, struct sockaddr* destAddress, socklen_t * addrLength, char sequenceNumber);

char getOpcode(char message[]);

void setOpcode(char message[], char opcode);

char getSequenceNumber(char message[]);

void setSequenceNumber (char message[], int seqNum);

int filenameCheck(char message[]);

int nextSequenceNum(int currentSequenceNum);

char* getFileNameFromRequest(char message[]);

int
main(int argc, char **argv)
{    
    FILE *myFile;
    int res;
    
    
    char receiveBuffer[2048];
    char sendBuffer[2048];
    char fileName[64];

    
    struct sockaddr_in myAddress, clientAddress;
    int socketNumber, x, recvlen;

    socklen_t addrLength = sizeof(clientAddress);
    
    /* Create Socket */
    socketNumber = socket(AF_INET, SOCK_DGRAM, 0);
    
    /* Socket Error Check */
    if(socketNumber < 0){
        perror("Server: Cannot create socket");
        exit(0);
    }
    
   //  Fill in the server's sockaddr struct 
    memset((char *) &myAddress, 0, sizeof(myAddress));
    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddress.sin_port = htons(PORT);
    
    /* Binding socket to address and port(Server Only) */
    /* NEED TO ERROR CHECK x */
    x = bind(socketNumber, (struct sockaddr *) &myAddress, sizeof(myAddress));
    
    //Allows recvFrom to timeout
    struct timeval read_timeout;
    read_timeout.tv_sec = TIMEOUT;
    read_timeout.tv_usec = 0;
    setsockopt(socketNumber, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
    
    /* Setup complete ready to receive data */
    for(;;){
        bzero(receiveBuffer, 2048);
        bzero(sendBuffer, 2048);
        recvlen = 0;
        recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr *) &clientAddress, &addrLength);
        if(recvlen > 0){
            switch(getOpcode(receiveBuffer)){
                case '1': //RRQ
                    printf("S: Received [Read Request]\n");
                    if (filenameCheck(receiveBuffer+2)==1){
                        
                        //Filename contains forbidden chars
                        printf("S: Pathnames are forbidden. Please provide a plain filename.\n");
                        
                        break;
                    }
                    sprintf(fileName, "server/%s", (receiveBuffer+2));
                    myFile = fopen(fileName, "rb");
                    if (myFile == NULL){
                        printf("S: File %s does not exist in the server directory.", receiveBuffer+2);
                        break;
                    }
                    rrqHandler(socketNumber, receiveBuffer, sendBuffer, (struct sockaddr*)&clientAddress, &addrLength, myFile);
                    fclose(myFile);
                    printf("S: File Read Complete\n");

                    break;
                case '2': //WRQ
                    printf("S: Received [Write Request]\n");
                    if (filenameCheck(receiveBuffer+2)==1){
                        //Filename contains forbidden chars
                        printf("S: Pathnames are forbidden. Please provide a plain filename.\n");
                        break;
                    }
                    
                    sprintf(fileName, "server/%s", (receiveBuffer+2));
                    myFile = fopen(fileName, "w");
                    //Need validity check for all fopens
                    // ACK to signal ready to receive
                    // First ACK of WRQ always block#0
                    sendACK(socketNumber, (struct sockaddr*)&clientAddress, &addrLength, '0');
                    printf("S: Sending Write Request ACK\n");
                    //Handles transmitted data
                    wrqHandler(socketNumber, receiveBuffer, (struct sockaddr*)&clientAddress, &addrLength, myFile);
                    
                    //All data written to file ready to close
                    fclose(myFile);
                    
                    printf("S: File Write Complete\n");
                    
                    break;
                default:
                    printf("S: Received [Unknown Request]\n");
                    printf("S: Please start a session by sending a RRW/WRQ.\n");
                    //Bad Request, needs to start session with RRQ/WRQ
                    break;
            }

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
    if (currentSequenceNum == (int)(SEQUENCEMAX)){
        return 0;
    }
    return currentSequenceNum+1;
}

/**
 * Returns the previous number in packet sequence. Returns SEQUENCEMAX if at 0
 * @param currentSequenceNum Current sequence number
 * @return SEQUENCEMAX if sequence at 0. Else, previous number in sequence
 */
int prevSequenceNum(int currentSequenceNum){
    if (currentSequenceNum == 0){
        return (int)SEQUENCEMAX;
    }
    return currentSequenceNum-1;
}

/**
 * Gets the opcode from the message header.
 * @param message The Message packet to read the opcode from
 * @return 1 for RRQ, 2 for WRQ, 3 for DATA, 4 for ACK, 5 for ERROR
 */
char getOpcode(char message[]){
    return message[1];
}

/**
 * Sets the opcode from the message header.
 * @param message The message packet to set the opcode for
 * @param opcode char: 1 for RRQ, 2 for WRQ, 3 for DATA, 4 for ACK, 5 for ERROR
 */
void setOpcode(char message[], char opcode){
    message[0] = '0';
    message[1] = opcode;
}

/**
 * Gets the sequence/block number from the message header.
 * @param message The Message packet to read the opcode from
 * @return
 */
char getSequenceNumber (char message[]){
    return message [3];
}

/**
 * Sets the sequence/block number of the message packet.
 * @param message The Message packet to set the sequence number for
 * @param seqNum The sequence number to set the packet to
 */
void setSequenceNumber (char message[], int seqNum){
    message[2] = '0';
    message[3] = '0'+ seqNum;
}


/**
 * Gets the filename from the message header of a RRQ or WRQ
 * @param message The message received
 * @return Filename from WRQ/RRQ header.
 */
char* getFileNameFromRequest(char message[]){
    char* filename;
    char* filenameEndPtr = strchr(message, '\0'); //First null char is end of filename
    int filenameEnd = filenameEndPtr-message+1;
    strncpy(filename, message+2, filenameEnd-2);
    return filename;
}

/**
 * Handles WRQ and accepts only DATA packets, then responds with ACK of the same sequence number.
 * If it doesn't receive a new DATA packet, it will resend ACK after TIMEOUT time.
 * @param socketNumber
 * @param messageBuffer
 * @param senderAddress
 * @param addrLength
 * @param myFile
 */
void wrqHandler(int socketNumber, char* messageBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ){
    int recvlen, retry;
    int currSequenceNumber = 0; 
    char fileBuf[MAXDATASIZE];
    retry = 0;
    //Keep sending message up to RETRYMAX times if there is no response
    while (retry < RETRYMAX){
        recvlen = 0;
        bzero(messageBuffer, 2048);
        recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr*)senderAddress, addrLength);
        //If there is data received
        if(recvlen > 0){
            //Since we got any response from other side we can reset retry
            retry = 0;
            //Check if the sequence number is correct
            if (getSequenceNumber(messageBuffer) != '0' + currSequenceNumber) {
                printf("S: Wrong Sequence Number!! Expected %c got %c\n", '0'+currSequenceNumber, getSequenceNumber(messageBuffer));
                continue; //Ignore if wrong sequence number
            }
            //Sequence # is correct so we can check the tag of the packet
            switch(getOpcode(messageBuffer)){
                //What we expected
                case '3': //Data tag
                    retry = 0;
                    memcpy(fileBuf, messageBuffer+4, recvlen-4);
                    fwrite(fileBuf, 1, recvlen-4, myFile);
                    if('0' == getSequenceNumber(messageBuffer)){
                        currSequenceNumber = 0;
                    }
                    else{
                        currSequenceNumber = 1;
                    }
                    printf("S: Received Data Block #%d\n", currSequenceNumber);
                    sendACK(socketNumber, senderAddress, addrLength, '0'+ currSequenceNumber);
                    printf("S: Sending ACK #%d\n", currSequenceNumber);
                    currSequenceNumber = nextSequenceNum(currSequenceNumber);
                    break;
                //If the tag was not a data tag we continue waiting
                default:
                    //Bad response, should be sending me some data
                    printf("S: Received non-DATA packet. Still waiting for DATA packet.\n");
                    break;
            }
        }
        
        //When the packet is data and it contains less than the max buffer size 
        //we are finished receiving data.
        if(recvlen < (MAXDATASIZE + 4) && messageBuffer[1] == '3' && recvlen > 0){
            printf("S: Finished receiving data\n");
            break;
        }
        //If we have not yet received a response we resend ACK
        if(recvlen <= 0){
            sendACK(socketNumber, senderAddress, addrLength, (char)prevSequenceNum(currSequenceNumber));
            retry++;
        }
    }
    //Maximum resends reached we have timed out
    if(retry == RETRYMAX){
        printf("S: Connection timed out.\n");
    }
}

void rrqHandler(int socketNumber, char* receiveBuffer, char* sendBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ){
    char fileBuf[MAXDATASIZE];
    int recvlen, retry, acked, x, currSequenceNumber = 0;
    int res = MAXDATASIZE;
    while(res == MAXDATASIZE) { //Send Data
        bzero(sendBuffer, 2048);
        res = fread(sendBuffer + 4, 1, MAXDATASIZE, myFile);
        if (res < 1) {
            //EOF and no more data
            printf("S: End of File Reached, terminating connection.\n");
            break;
        }
        retry = 0;
        acked = 0;
        while (retry < RETRYMAX && acked == 0) { //Retry sending 10 times
            setOpcode(sendBuffer, '3'); //Sending DATA packets
            setSequenceNumber(sendBuffer, currSequenceNumber);
            x = sendto(socketNumber, sendBuffer, res+4, 0, (struct sockaddr *) senderAddress, *addrLength);
            printf("S: Sending data block #%d\n", currSequenceNumber);
            
            recvlen = 0;
            bzero(receiveBuffer, 2048);
            recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr *) senderAddress, addrLength);
            if (recvlen > 0) {
                retry = 0;
                switch (getOpcode(receiveBuffer)) {
                    case '4': //ACK tag
                        if (getSequenceNumber(receiveBuffer) != '0'+currSequenceNumber){
                            //Wrong sequence number
                            printf("S: Received packet with wrong sequence number.\n");
                            printf("S: Sequence number was #%c, should be #%c.\n", getSequenceNumber(receiveBuffer), '0'+currSequenceNumber);
                        } 
                        else{
                            acked = 1;
                            printf("S: Received ACK #%d\n", currSequenceNumber);
                            currSequenceNumber = nextSequenceNum(currSequenceNumber);
                            
                        }
                        break;
                    default:
                        //Bad response, should be ACK tag
                        printf("S: Received non-ACK packet. Still waiting for ACK packet.\n");
                        break;
                }
            }
            else{ 
                retry++; //Break out of loop, retry sending data
            }
            
        }
        // When max retry reached drop connection
        if(retry == RETRYMAX){
            printf("S: Connection timed out.\n");
            break;
        }
    }
    //Finished sending Data, session end notice
    printf("S: Ending Transmission.\n");
}


void sendACK(int socketNumber, struct sockaddr* destAddress, socklen_t * addrLength, char sequenceNumber){
    char messageBuffer[2048];
    setOpcode( messageBuffer, '4'); //Sending ACK packets
    if('0' == sequenceNumber){
        setSequenceNumber( messageBuffer, 0);
    }
    else{
        setSequenceNumber( messageBuffer, 1);
    }
    
    //TODO: Need to error check result of sendto
    sendto(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) destAddress, *addrLength);
    
}
