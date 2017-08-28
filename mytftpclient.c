#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
//docker run --rm -it -v c:/Users/droid/dockerProject:/home/ prodromou87/ucsd_cse123

int main(void)
{
    
    char fileName[64];
    
    printf("Hello, world!\n");
    struct sockaddr_in serverAddress;
    struct hostent *hp;
    int fd, x, recvlen;
    socklen_t addrLength = sizeof(serverAddress);
    char buf[2048];
    buf[0] = 'H';
    buf[1] = '1';
    buf[2] = 'l';
    buf[3] = 'l';
    buf[4] = '\0';
    
    /* Create Socket */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    /* Socket Error Check */
    if(fd < 0){
        perror("Client: Cannot create socket");
        exit(0);
    }
    /* Fill in the server's sockaddr struct */
    memset((char *) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(61003);
    
    /* Get server address (Need to check validity)*/
    hp = gethostbyname("localhost");
    
    /* Fill server sockaddr struct */
    memcpy((void *)&serverAddress.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    /* Now can send request */
    x = sendto(fd, buf, strlen(buf), 0, (struct sockaddr*)&serverAddress, addrLength);
    printf("Message sent to server\n");
    
    
    
    for(;;){
        recvlen = recvfrom(fd, buf, 2048, 0, (struct sockaddr*)&serverAddress, &addrLength);
        if(recvlen > 0){
            buf[recvlen] = '\0';
            printf("Message from server: %s\n", buf);
            
        }
    }
    
    return 0;
}