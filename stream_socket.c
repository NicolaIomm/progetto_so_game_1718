#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "so_game_protocol.h"

//#include <sys/types.h>
//#include <sys/socket.h>
#include <arpa/inet.h>  // hton() , inet_addr()

#pragma GCC diagnostic ignored "-Wwrite-strings"

#define DEBUG 0
#define INCOMING_DATA_SIZE 1000000

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

	bytes_sent = 0;
	while (bytes_sent < data_len){
		ret = send(socket, data+bytes_sent, data_len-bytes_sent, 0);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 0)
			print_err("Error while writing on socket.");
		bytes_sent += ret;
	}

  if (DEBUG){
      printf("Stream_Socket:\n");
      printf("\tData size: %d\n", data_len);
      printf("\tData sent: %d\n", bytes_sent);
}

	return bytes_sent;
}

int receivePacketTCP(int socket, char* data){
	int ret;
	int bytes_received;
	int header_size = sizeof(PacketHeader);
  int body_size;

	bytes_received = 0;
	while (bytes_received < header_size){
		ret = recv(socket, data+bytes_received, header_size-bytes_received, 0);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 0)
			print_err("Error while reading from socket.");
		bytes_received += ret;
	}

  body_size = (((PacketHeader*)data)->size - header_size);

	bytes_received = 0;
	while (bytes_received < body_size){
		ret = recv(socket, data+header_size+bytes_received, body_size-bytes_received, 0);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret < 0)
			print_err("Error while writing on socket.");
		bytes_received += ret;
	}

  if (DEBUG){
      printf("Stream_Socket:\n");
      printf("\tData size: %d\n", header_size + body_size);
      printf("\tData received: %d\n", bytes_received);
  }


	return bytes_received;
}

int sendPacketUDP(int socket, char* data, int data_len, struct sockaddr* server_addr, socklen_t server_len){
  int bytes_sent = 0;

  bytes_sent = sendto(socket, data, data_len, 0, server_addr, server_len);

  if (DEBUG){
    printf("Data to send: %s\n", data);
    printf("Data size: %d\n", bytes_sent);
  }

  return bytes_sent;
}

int receivePacketUDP(int socket, char* data, struct sockaddr* server_addr, socklen_t* server_len){
  int bytes_received = 0;

  bytes_received = recvfrom(socket, data, INCOMING_DATA_SIZE, MSG_WAITALL, server_addr, server_len);

  if (DEBUG){
    printf("Data received: %s\n", data);
    printf("Data size: %d\n", bytes_received);
  }

  return bytes_received;
}
