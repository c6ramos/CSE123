#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>

#define SEQUENCEMAX 1
#define PORT 61003
#define MAXDATASIZE 512
#define TIMEOUT 1000
#define RETRYMAX 10

//Receive handler
void receiveHandler(int fd, char* buf, struct sockaddr* senderAddress, socklen_t * addrLength, FILE * myFile );
//Send ACK function
void sendACK(int fd, struct sockaddr* destAddress, socklen_t * addrLength);

char getOpcode(char message[]);

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
        recvlen = 0;
        recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) &clientAddress, &addrLength);
        if(recvlen > 0){
            switch(getOpcode(messageBuffer)){
                case '1': //RRQ
                    if (filenameCheck(getFileNameFromRequest(messageBuffer))==1){
                        //TODO: Filename contains forbidden chars
                        return 1;
                    }
                    sprintf(fileName, "server/%s.txt", (messageBuffer+2));
                    myFile = fopen(fileName, "r");
                    rrqHandler(socketNumber, messageBuffer, (struct sockaddr*)&clientAddress, &addrLength, myFile);
                    fclose(myFile);
                    break;
                case '2': //WRQ
                    if (filenameCheck(getFileNameFromRequest(messageBuffer))){
                        //TODO: Filename contains forbidden chars
                        return 1;
                    }
                    
                    myFile = fopen("server/myFile2.txt", "w");  //Arbitrary name need to replace
                    
                    // ACK to signal ready to receive
                    sendACK(socketNumber, (struct sockaddr*)&clientAddress, &addrLength);
                    
                    //Handles transmitted data
                    wrqHandler(socketNumber, messageBuffer, (struct sockaddr*)&clientAddress, &addrLength, myFile);
                    
                    //All data written to file ready to close
                    fclose(myFile);
                    
                    break;
                default:
                    //TODO: Bad Request, needs to start session with RRQ/WRQ
                    break;
            }
            
            messageBuffer[25] = '\0';
            sendto(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) &clientAddress, addrLength);
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
 * Gets the opcode from the message header.
 * @return 1 for RRQ, 2 for WRQ, 3 for DATA, 4 for ACK, 5 for ERROR
 */
char getOpcode(char message[]){
    return message[1];
}

/**
 * Sets the opcode from the message header.
 * 1 for RRQ, 2 for WRQ, 3 for DATA, 4 for ACK, 5 for ERROR
 */
void setOpcode(char message[], char opcode){
    message[1] = opcode;
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

void wrqHandler(int socketNumber, char* messageBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ){
    int recvlen;
    char fileBuf[MAXDATASIZE];

    for(;;){
        recvlen = 0;
        recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr*)senderAddress, addrLength);
        if(recvlen > 0){
            switch(getOpcode(messageBuffer)){
                case '3': //Data tag
                    memcpy(fileBuf, messageBuffer+4, recvlen-4);
                    fwrite(fileBuf, 1, recvlen-4, myFile);
                    sendACK(socketNumber, senderAddress, addrLength);
                    break;
                default:
                    //TODO: Bad response, should be sending me some data
                    break;
            }
        }
        //Change to 516 once block # added
        if(recvlen < MAXDATASIZE && recvlen > 0 && messageBuffer[1] == '3')
            break;
    }
}

void rrqHandler(int socketNumber, char* messageBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ){
    char fileBuf[MAXDATASIZE];
    int recvlen, retry, acked;
    int res = MAXDATASIZE;
    time_t start;

    while(res == MAXDATASIZE) { //Send Data
        res = fread(messageBuffer + 2, 1, MAXDATASIZE, myFile);
        if (res < 1) {
            //TODO: EOF and no more data
            break;
        }
        retry = 0;
        while (retry < RETRYMAX) { //Retry sending 10 times
            setOpcode(messageBuffer, '3'); //Sending DATA packets
            sendto(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) &senderAddress, addrLength);
            start = clock();   //Start timer
            acked = 0;
            while (acked == 0) { //Wait for ACK for timeout seconds
                recvlen = 0;
                recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) senderAddress, addrLength);
                if (recvlen > 0) {
                    switch (getOpcode(messageBuffer)) {
                        case '4': //ACK tag
                            //TODO: Need to check for correct block#
                            acked = 1;
                            retry = RETRYMAX; //Break out of retry loop
                            break;
                        default:
                            //TODO: Bad response, should be ACK tag
                            break;
                    }
                }
                if (end - clock() > TIMEOUT) { //If wait for ACK timeouts
                    retry++; //Break out of loop, retry sending data
                    break;
                }
            }
        }
    }
    //TODO: Finished sending Data, session end notice?
}

void sendACK(int fd, struct sockaddr* destAddress, socklen_t * addrLength){
    char messageBuffer[2048];
    setOpcode( messageBuffer, '4'); //Sending ACK packets
    //TODO: Need to add block #
    
    //TODO: Need to error check result of sendto
    sendto(fd, messageBuffer, 2048, 0, (struct sockaddr *) destAddress, *addrLength);
}