#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>


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

void setOpcode(char message[], char opcode);


int main(int argc, char **argv)
{
    
    char fileName[64];
    
    FILE *myFile;
    int res;

    char receiveBuffer[2048];
    char sendBuffer[2048];
    
    struct sockaddr_in serverAddress;
    struct hostent *hp;
    int socketNumber, x, recvlen, requestReceived;
    socklen_t addrLength = sizeof(serverAddress);
    
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
    
    //Allows recvFrom to timeout
    struct timeval read_timeout;
    read_timeout.tv_sec = TIMEOUT;
    read_timeout.tv_usec = 0;
    setsockopt(socketNumber, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
    
    //Check if user input is valid
    if(argc == 3 && filenameCheck(argv[2]) == 0){

        //Check if request is a write
        if(strcmp( argv[1], "-w") == 0){
            /* Now can send request */
            printf("Request sent to server\n");
            sprintf(fileName, "client/%s", argv[2]);
            bzero(sendBuffer, 2048);
            //Set message opcode
            setOpcode(sendBuffer, '2');
            //Set filename in buffer
            strcpy(sendBuffer+2, argv[2]);
            /* Now send request */
            x = sendto(socketNumber, sendBuffer, strlen(sendBuffer), 0, (struct sockaddr*)&serverAddress, addrLength);

            requestReceived = 0;
            while (requestReceived == 0){
                recvlen = 0;
                bzero(receiveBuffer, 2048);
                recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr*)&serverAddress, &addrLength);
                if(recvlen > 0){
                    if (getSequenceNumber(receiveBuffer) != '0'){
                        //Initial ACK must be block#0
                        printf("C: ACK response to WRQ must have sequence #0. Received sequence #%c.\n", getSequenceNumber(receiveBuffer));
                        continue;
                    }
                    switch(getOpcode(receiveBuffer)){
                        case '4': //ACK received
                            printf("C: Received WRQ ACK\n");
                            requestReceived = 1;
                            break;
                        default:
                            //Bad response, should be sending me some data
                            printf("C: Received non-DATA packet. Still waiting for DATA packet.\n");
                            break;
                    }
                }
                else{
                    printf("Timed Out: Write request not acknowledged by server.\n");
                    return 0;
                }
            }
            //Request Acknowledged start reading file and start sending data
            myFile = fopen(fileName, "rb");
            rrqHandler(socketNumber, receiveBuffer, sendBuffer, (struct sockaddr*)&serverAddress, &addrLength, myFile);
            fclose(myFile);
            printf("C: Write Request Complete\n");
        }

        //If request was a read
        else if(strcmp(argv[1], "-r") == 0){
            
            
            //Destination Path
            sprintf(fileName, "client/%s", argv[2]);
            myFile = fopen(fileName, "w");  
            bzero(sendBuffer, 2048);
            //Set message opcode
            setOpcode(sendBuffer, '1');
            //Set filename in buffer
            strcpy(sendBuffer+2, argv[2]);
            /* Now can send request */
            sendto(socketNumber, sendBuffer, strlen(sendBuffer), 0, (struct sockaddr*)&serverAddress, addrLength);
            printf("C: Read request sent to server\n");
            
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
void setSequenceNumber (char message[], int seqNum){
    message[2] = '0';
    message[3] = '0'+ seqNum;
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
    int currSequenceNumber = 0; //Packets start at sequence#1 after sending initial ACK#0

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
                printf("C: Wrong Sequence Number!! Expected %c got %c\n", '0'+currSequenceNumber, getSequenceNumber(messageBuffer));
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
                    printf("C: Received Data Block #%d\n", currSequenceNumber);
                    sendACK(socketNumber, senderAddress, addrLength, '0'+currSequenceNumber);
                    printf("C: Sending ACK #%d\n", currSequenceNumber);
                    currSequenceNumber = nextSequenceNum(currSequenceNumber);
                    break;
                //If the tag was not a data tag we continue waiting
                default:
                    //Bad response, should be sending me some data
                    printf("C: Received non-DATA packet. Still waiting for DATA packet.\n");
                    break;
            }
        }
        
        //When the packet is data and it contains less than the max buffer size 
        //we are finished receiving data.
        if(recvlen < (MAXDATASIZE + 4) && messageBuffer[1] == '3' && recvlen > 0){
            printf("C: Finished receiving data\n");
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
        printf("C: Connection timed out.\n");
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
            printf("C: End of File Reached, terminating connection.\n");
            break;
        }
        retry = 0;
        acked = 0;
        while (retry < RETRYMAX && acked == 0) { //Retry sending 10 times
            setOpcode(sendBuffer, '3'); //Sending DATA packets
            setSequenceNumber(sendBuffer, currSequenceNumber);
            x = sendto(socketNumber, sendBuffer, res+4, 0, (struct sockaddr *) senderAddress, *addrLength);
            printf("C: Sending data block #%d\n", currSequenceNumber);
            bzero(receiveBuffer, 2048);
            recvlen = 0;
            recvlen = recvfrom(socketNumber, receiveBuffer, 2048, 0, (struct sockaddr *) senderAddress, addrLength);
            if (recvlen > 0) {
                retry = 0;
                switch (getOpcode(receiveBuffer)) {
                    case '4': //ACK tag
                        if (getSequenceNumber(receiveBuffer) != '0'+currSequenceNumber){
                            //Wrong sequence number
                            printf("C: Received packet with wrong sequence number.\n");
                            printf("C: Sequence number was #%c, should be #%c.\n", getSequenceNumber(receiveBuffer), '0'+currSequenceNumber);
                        } 
                        else{
                            acked = 1;
                            printf("C: Received ACK #%d\n", currSequenceNumber);
                            currSequenceNumber = nextSequenceNum(currSequenceNumber);
                            
                        }
                        break;
                    default:
                        //Bad response, should be ACK tag
                        printf("C: Received non-ACK packet. Still waiting for ACK packet.\n");
                        break;
                }
            }
            else{ 
                retry++; //Break out of loop, retry sending data
            }
            
        }
        // When max retry reached drop connection
        if(retry == RETRYMAX){
            printf("C: Connection timed out.\n");
            break;
        }
    }
    //Finished sending Data, session end notice
    printf("C: Ending Transmission.\n");
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

