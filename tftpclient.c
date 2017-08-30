#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
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

void setOpcode(char message[], char opcode);


int main(int argc, char **argv)
{
    
    char fileName[64];
    
    FILE *myFile;
    int res;

    
    struct sockaddr_in serverAddress;
    struct hostent *hp;
    int socketNumber, x, recvlen, requestReceived;
    socklen_t addrLength = sizeof(serverAddress);
    char receiveBuffer[2048];
    char sendBuffer[2048];
    time_t start;
    
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
    
    //Check if user input is valid
    if(argc == 3 && filenameCheck(argv[2]) == 0){
        
        //Check if request is a write
        if(strcmp( argv[1], "-w") == 0){
            /* Now can send request */
            printf("Request sent to server\n");
            sprintf(fileName, "client/%s", argv[2]);
            
            //Set message opcode
            setOpcode(sendBuffer, '2');
            //Set filename in buffer
            strcpy(sendBuffer+2, argv[2]);
            /* Now send request */
            x = sendto(socketNumber, sendBuffer, strlen(sendBuffer), 0, (struct sockaddr*)&serverAddress, addrLength);
            //Wait for ack
            start = clock();
            requestReceived = 0;
            while (requestReceived == 0){
                recvlen = 0;
                recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr*)&serverAddress, &addrLength);
                if(recvlen > 0){
                    if (getSequenceNumber(receiveBuffer) != '0'){
                        //TODO: Initial ACK must be block#0
                        continue;
                    }
                    switch(getOpcode(receiveBuffer)){
                        case '4': //ACK received
                            requestReceived = 1;
                            break;
                        default:
                            //TODO: Bad response, should be sending me some data
                            break;
                    }
                }
                else if(clock() - start > TIMEOUT){
                    printf("Timeout: Write request not acknowledged by server.\n");
                    return 0;
                }
            }
            //Request Acknowledged start reading file and start sending data
            myFile = fopen(fileName, "r");
            rrqHandler(socketNumber, receiveBuffer, sendBuffer, (struct sockaddr*)&serverAddress, &addrLength, myFile);
            fclose(myFile);
            printf("C: Write Request Complete\n");
        }

        //If request was a read
        else if(strcmp(argv[1], "-r") == 0){
            
            //Destination Path
            sprintf(fileName, "client/%s", argv[2]);
            myFile = fopen(fileName, "w");  
            
            //Set message opcode
            setOpcode(sendBuffer, '1');
            //Set filename in buffer
            strcpy(sendBuffer+2, argv[2]);
            /* Now can send request */
            sendto(socketNumber, sendBuffer, strlen(sendBuffer), 0, (struct sockaddr*)&serverAddress, addrLength);
            printf("Read request sent to server\n");
            
            //Handles transmitted data
            wrqHandler(socketNumber, receiveBuffer, (struct sockaddr*)&serverAddress, &addrLength, myFile);

            //All data written to file ready to close
            fclose(myFile);
            
            printf("C: Read Request Complete\n");
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


void wrqHandler(int socketNumber, char* messageBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ){
    int recvlen, retry, currSequenceNumber = 0;
    time_t start;
    
    char fileBuf[MAXDATASIZE];
    //Need to also check packet # for repeats
    retry = 0;
    start = clock();   //Start timer
  //  end = start + TIMEOUT;
    while (retry < RETRYMAX){
        recvlen = 0;
        recvlen = recvfrom(socketNumber, messageBuffer, 2048, 0, (struct sockaddr*)senderAddress, addrLength);
        if(recvlen > 0){
            if (getSequenceNumber(messageBuffer) != currSequenceNumber){
                //TODO: Wrong sequence Number
                //Keep listening
                continue;
            }
            switch(getOpcode(messageBuffer)){
                case '3': //Receiving DATA
                    retry = 0;
                    start = clock();
                    memcpy(fileBuf, messageBuffer+4, recvlen-4);
                    fwrite(fileBuf, 1, recvlen-4, myFile);
                    printf("C: Received Block #%d of Data\n", currSequenceNumber);
                    sendACK(socketNumber, senderAddress, addrLength, (char)currSequenceNumber);
                    printf("C: Sending ACK #%d\n", currSequenceNumber);
                    currSequenceNumber = nextSequenceNum( currSequenceNumber);
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
            printf("C: Resending ACK #%d\n", prevSequenceNum(currSequenceNumber));
        }
    }
    
}

void rrqHandler(int socketNumber, char* receiveBuffer, char* sendBuffer, struct sockaddr* senderAddress, socklen_t * addrLength, FILE* myFile ){
    char fileBuf[MAXDATASIZE];
    int recvlen, retry, acked, x = 0;
    int currSequenceNumber = 1;
    int res = MAXDATASIZE;
    time_t start;
    while(res == MAXDATASIZE) { //Send Data
        //Temporary to fill in packet # bits
//        sendBuffer[2] = '0';
//        sendBuffer[3] = '0';
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
            printf("C: Sending block #%d of data\n", currSequenceNumber);
            start = clock();   //Start timer
            acked = 0;
            while (acked == 0) { //Wait for ACK for timeout seconds
                recvlen = 0;
                recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr *) senderAddress, addrLength);
                if (recvlen > 0) {
                    if (getSequenceNumber(receiveBuffer) != currSequenceNumber){
                        //TODO: ACK block# should be same as the one in send DATA
                        //Keep waiting for ACK block
                        continue;
                    }
                    switch (getOpcode(receiveBuffer)) {
                        case '4': //ACK tag
                            //TODO: Need to check for correct block#
                            acked = 1;
                            retry = RETRYMAX; //Break out of retry loop
                            printf("C: Received ACK #%d\n", currSequenceNumber);
                            currSequenceNumber = nextSequenceNum(currSequenceNumber);
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
    messageBuffer[1] = '4';
    //Need to add block #
    setSequenceNumber(messageBuffer, sequenceNumber);
    
    //Need to error check result of sendto
    sendto(socketNumber, messageBuffer, 2048, 0, (struct sockaddr *) destAddress, *addrLength);
}