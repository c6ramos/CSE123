#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

int main(void)
{

    
    FILE *myFile;
    FILE *newFile;
    char buf[26];
    int res;
    
    printf("we will now read the file\n");
    
    
    myFile = fopen("server/myFile.txt", "r");
    newFile = fopen("client/newFile.txt", "w");
    if(myFile != NULL){
        res = fread(buf, 1, 25, myFile);
        buf[res] = '\0';
        printf("Read %u bytes.\n", res);
        printf("Buf contains: %s", buf);
        fwrite(buf, 1, res, newFile);
        while(res == 25){
            res = fread(buf, 1, 25, myFile);
            buf[res] = '\0';
            printf("Read %u bytes.\n", res);
            printf("Buf contains: %s", buf);
            fwrite(buf, 1, res, newFile);
        }
        fclose(myFile);
        
        
        fclose(newFile);
    }
    else{
        printf("Failed to read file");
    }
    
    return 0;
}
