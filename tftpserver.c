#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>

#define SEQUENCEMAX 1
#define PORT 61003
#define MAXDATASIZE 512
#define TIMEOUT 1000
#define RETRYMAX 10


void wrqHandler(int socketNumber, char* messageBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile );

void rrqHandler(int socketNumber, char* receiveBuffer, char* sendBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ); 
//Send ACK function
void sendACK(int socketNumber, struct sockaddr* destAddress, socklen_t * addrLength, char sequenceNumber);

char getOpcode(char message[]);

void setOpcode(char message[], char opcode);

char getSequenceNumber(char message[]);

void setSequenceNumber (char message[], char seqNum);

int filenameCheck(char message[]);

int nextSequenceNum(int currentSequenceNum);

char* getFileNameFromRequest(char message[]);

int
main(int argc, char **argv)
{    
    FILE *myFile;
    int res;
    char fileName[64];
    
    struct sockaddr_in myAddress, clientAddress;
    int socketNumber, x, recvlen;
    char receiveBuffer[2048];
    char sendBuffer[2048];
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
        recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr *) &clientAddress, &addrLength);
        if(recvlen > 0){
            switch(getOpcode(receiveBuffer)){
                case '1': //RRQ
                    printf("S: Received [Read Request]\n");
                    if (filenameCheck(getFileNameFromRequest(receiveBuffer+2))==1){
                        //TODO: Filename contains forbidden chars
                        return 1;
                    }
                    sprintf(fileName, "server/%s", (receiveBuffer+2));
                    myFile = fopen(fileName, "r");
                    rrqHandler(socketNumber, receiveBuffer, sendBuffer, (struct sockaddr*)&clientAddress, &addrLength, myFile);
                    fclose(myFile);
                    printf("S: File Read Complete\n");
                    break;
                case '2': //WRQ
                    printf("S: Received [Write Request]\n");
                    if (filenameCheck(getFileNameFromRequest(receiveBuffer+2)) == 1){
                        //TODO: Filename contains forbidden chars
                        return 1;
                    }
                    
                    sprintf(fileName, "server/%s", (receiveBuffer+2));
                    myFile = fopen(fileName, "w");
                    //Need validity check for all fopens
                    
                    // ACK to signal ready to receive
                    // First ACK of WRQ always block#0
                    sendACK(socketNumber, (struct sockaddr*)&clientAddress, &addrLength, '0');
                    
                    //Handles transmitted data
                    wrqHandler(socketNumber, receiveBuffer, (struct sockaddr*)&clientAddress, &addrLength, myFile);
                    
                    //All data written to file ready to close
                    fclose(myFile);
                    
                    printf("S: File Write Complete\n");
                    
                    break;
                default:
                    printf("S: Received [Unknown Request]\n");
                    //TODO: Bad Request, needs to start session with RRQ/WRQ
                    break;
            }
            
            /*sendBuffer[25] = '\0';
            sendto(socketNumber, sendBuffer, 2048, 0, (struct sockaddr *) &clientAddress, addrLength);*/
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
void setSequenceNumber (char message[], char seqNum){
    message[2] = '0';
    message[3] = seqNum;
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
    time_t start;
    int currSequenceNumber = 1; //Packets start at sequence#1 after sending initial ACK#0

    char fileBuf[MAXDATASIZE];
    retry = 0;
    start = clock();   //Start timer
  //  end = start + TIMEOUT;
    while (retry < RETRYMAX){
        recvlen = 0;
        recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr*)senderAddress, addrLength);
        if(recvlen > 0){
            if (getSequenceNumber(messageBuffer) != currSequenceNumber) {
                continue; //Ignore if wrong sequence number
            }
            switch(getOpcode(messageBuffer)){
                case '3': //Data tag
                    retry = 0;
                    start = clock();
                    memcpy(fileBuf, messageBuffer+4, recvlen-4);
                    fwrite(fileBuf, 1, recvlen-4, myFile);
                    currSequenceNumber = (int)getSequenceNumber(messageBuffer);
                    printf("S: Received Block #%d of Data\n", currSequenceNumber);
                    sendACK(socketNumber, senderAddress, addrLength, (char)currSequenceNumber);
                    printf("S: Sending ACK #%d\n", currSequenceNumber);
                    currSequenceNumber = nextSequenceNum(currSequenceNumber);
                    break;
                default:
                    //TODO: Bad response, should be sending me some data
                    break;
            }
        }
        
        //Change to 516 once block # added
        if(recvlen < (MAXDATASIZE + 4) && messageBuffer[1] == '3')
            break;
        if(clock() - start > TIMEOUT){
            sendACK(socketNumber, senderAddress, addrLength, (char)prevSequenceNum(currSequenceNumber));
            retry++;
            start = clock();
        }
    }
}

void rrqHandler(int socketNumber, char* receiveBuffer, char* sendBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ){
    char fileBuf[MAXDATASIZE];
    int recvlen, retry, acked, x, currSequenceNumber = 0;
    int res = MAXDATASIZE;
    time_t start;
    while(res == MAXDATASIZE) { //Send Data
        //Temporary to fill in packet # bits
        sendBuffer[2] = '0';
        sendBuffer[3] = '0';
        res = fread(sendBuffer + 4, 1, MAXDATASIZE, myFile);
        if (res < 1) {
            //TODO: EOF and no more data
            break;
        }
        retry = 0;
        while (retry < RETRYMAX) { //Retry sending 10 times
            setOpcode(sendBuffer, '3'); //Sending DATA packets
            setSequenceNumber(sendBuffer, (char)currSequenceNumber);
            x = sendto(socketNumber, sendBuffer, res+4, 0, (struct sockaddr *) senderAddress, *addrLength);
            printf("S: Sending block #%d of data\n", currSequenceNumber);
            start = clock();   //Start timer
            acked = 0;
            while (acked == 0) { //Wait for ACK for timeout seconds
                recvlen = 0;
                recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr *) senderAddress, addrLength);
                if (recvlen > 0) {
                    if (getSequenceNumber(receiveBuffer) != currSequenceNumber){
                        //TODO: Wrong sequence number
                        continue;
                    };
                    switch (getOpcode(receiveBuffer)) {
                        case '4': //ACK tag
                            acked = 1;
                            retry = RETRYMAX; //Break out of retry loop
                            currSequenceNumber = nextSequenceNum(currSequenceNumber);
                            printf("S: Received ACK #%d\n", currSequenceNumber);
                            break;
                        default:
                            //TODO: Bad response, should be ACK tag
                            break;
                    }
                }
               // if (end - clock() > TIMEOUT) { //If wait for ACK timeouts
                if (clock() - start > TIMEOUT) { //If wait for ACK timeouts
                    retry++; //Break out of loop, retry sending data
                    break;
                }
            }
        }
    }
    //TODO: Finished sending Data, session end notice?
}


void sendACK(int socketNumber, struct sockaddr* destAddress, socklen_t * addrLength, char sequenceNumber){
    char messageBuffer[2048];
    setOpcode( messageBuffer, '4'); //Sending ACK packets
    //TODO: Need to add block #
    setSequenceNumber( messageBuffer, sequenceNumber);
    
    //TODO: Need to error check result of sendto
    sendto(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) destAddress, *addrLength);
}