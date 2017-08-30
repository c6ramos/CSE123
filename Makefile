CC=gcc
CFLAGS = -g 
# uncomment this for SunOS
# LIBS = -lsocket -lnsl

all: tftpserver tftpclient

tftpserver: tftpserver.o 
	$(CC) -o tftpserver tftpserver.o $(LIBS)

tftpclient: tftpclient.o 
	$(CC) -o tftpclient tftpclient.o $(LIBS)

tftpserver.o: tftpserver.c 

tftpclient.o: tftpclient.c

clean:
	rm -f tftpserver tftpclient tftpserver.o tftpclient.o 