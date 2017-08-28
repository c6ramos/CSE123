#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>

int
main(int argc, char **argv)
{    
    FILE *myFile;
    int res;
    char fileName[64];
    
    struct sockaddr_in myAddress, clientAddress;
    int fd, x, recvlen;
    char buf[2048];
    socklen_t addrLength = sizeof(clientAddress);
    
    /* Create Socket */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    /* Socket Error Check */
    if(fd < 0){
        perror("Server: Cannot create socket");
        exit(0);
    }
    
    /* Fill in the server's sockaddr struct */
    memset((char *) &myAddress, 0, sizeof(myAddress));
    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddress.sin_port = htons(61003);
    
    /* Binding socket to address and port(Server Only) */
    /* NEED TO ERROR CHECK x */
    x = bind(fd, (struct sockaddr *) &myAddress, sizeof(myAddress));
    
    /* Setup complete ready to receive data */
    for(;;){
        recvlen = recvfrom(fd, buf, 2048, 0, (struct sockaddr *) &clientAddress, &addrLength);
        if(recvlen > 0){
            switch(buf[1]){
                case '1':
                    if (filenameCheck(fileName)==1){
                        //Filename contains forbidden chars
                        return 1;
                    }
                    sprintf(fileName, "server/%s.txt", (buf+2));
                    myFile = fopen(fileName, "r");
                    res = fread(buf+2, 1, 25, myFile);
                    break;
                case '2':
                    buf[1] = 'B';
                    break;
                case '3':
                    break;
            }
            
            buf[25] = '\0';
            //printf("Message from Client: %s", buf);
            sendto(fd, buf, 2048, 0, (struct sockaddr *) &clientAddress, addrLength);
        }
    }
    
    return 0;
}

int filenameCheck(char str[]){
    char forbiddenChars[11] = {'/', '\\', ':', '*', '\?','\"','<','>','|','~','\0'};

    for (int i = 0; i < strlen(forbiddenChars); i++)
        if (strchr(str, forbiddenChars[i]) != NULL){
            return 1;
        }
    return 0;
}