#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>  // hton() , inet_addr()

#pragma GCC diagnostic ignored "-Wwrite-strings"

#define DEBUG 0

void print_err(char* msg){
  fprintf(stderr, "%s\n", msg);
  exit(-1);
}

char* get_hostname_from_arg(char* arg){
  char* s = strdup(arg);
  char* ret = strsep(&s, ":");
  return ret;
}

int get_portno_from_arg(char* arg){
  int portno = atoi(strchr(arg, ':') + 1);
  return portno;
}

int sendPacketTCP(int socket, char* data, int data_len){
	int ret;
	int bytes_sent;
	uint32_t size = htonl(data_len);

	bytes_sent = 0;
	while (bytes_sent < sizeof(uint32_t)){
		ret = send(socket, (char*)(&size)+bytes_sent, sizeof(uint32_t)-bytes_sent, 0);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 0)
			print_err("Error while writing on socket.");
		bytes_sent += ret;
	}

    if (DEBUG){
        printf("Stream_Socket:\n");
        printf("\tData size: %d\n", data_len);
    }

	bytes_sent = 0;
	while (bytes_sent < data_len){
		ret = send(socket, data+bytes_sent, data_len-bytes_sent, 0);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 0)
			print_err("Error while writing on socket.");
		bytes_sent += ret;
	}

    if (DEBUG)
        printf("\tData sent: %d\n", bytes_sent);

	return bytes_sent;
}

int receivePacketTCP(int socket, char* data){
	int ret;
	int bytes_received;
	uint32_t size;

	bytes_received = 0;
	while (bytes_received < sizeof(uint32_t)){
		ret = recv(socket, (char*)(&size)+bytes_received, sizeof(uint32_t)-bytes_received, 0);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 0)
			print_err("Error while reading from socket.");
		bytes_received += ret;
	}

	int data_len = ntohl(size);

    if (DEBUG){
        printf("Stream_Socket:\n");
        printf("\tData size: %d\n", data_len);
    }

	bytes_received = 0;
	while (bytes_received < data_len){
		ret = recv(socket, data+bytes_received, data_len-bytes_received, 0);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 0)
			print_err("Error while writing on socket.");
		bytes_received += ret;
	}

    if (DEBUG)
        printf("\tData received: %d\n", bytes_received);

	return bytes_received;
}
