#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include <pthread.h>

#include <sys/socket.h> // socket(), connect()
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h>  // hton() , inet_addr()

#include "so_game_protocol.h"
#include "stream_socket.h"

#define INCOMING_DATA_SIZE 1000000

World world;
Vehicle* vehicle; // The vehicle

/*
int window;
WorldViewer viewer;

void keyPressed(unsigned char key, int x, int y){
  switch(key){
  case 27:
    glutDestroyWindow(window);
    exit(0);
  case ' ':
    vehicle->translational_force_update = 0;
    vehicle->rotational_force_update = 0;
    break;
  case '+':
    viewer.zoom *= 1.1f;
    break;
  case '-':
    viewer.zoom /= 1.1f;
    break;
  case '1':
    viewer.view_type = Inside;
    break;
  case '2':
    viewer.view_type = Outside;
    break;
  case '3':
    viewer.view_type = Global;
    break;
  }
}

void specialInput(int key, int x, int y) {
  switch(key){
  case GLUT_KEY_UP:
    vehicle->translational_force_update += 0.1;
    break;
  case GLUT_KEY_DOWN:
    vehicle->translational_force_update -= 0.1;
    break;
  case GLUT_KEY_LEFT:
    vehicle->rotational_force_update += 0.1;
    break;
  case GLUT_KEY_RIGHT:
    vehicle->rotational_force_update -= 0.1;
    break;
  case GLUT_KEY_PAGE_UP:
    viewer.camera_z+=0.1;
    break;
  case GLUT_KEY_PAGE_DOWN:
    viewer.camera_z-=0.1;
    break;
  }
}

void display(void) {
  WorldViewer_draw(&viewer);
}

void reshape(int width, int height) {
  WorldViewer_reshapeViewport(&viewer, width, height);
}

void idle(void) {
  World_update(&world);
  usleep(30000);
  glutPostRedisplay();
  
  // decay the commands
  vehicle->translational_force_update *= 0.999;
  vehicle->rotational_force_update *= 0.7;
}
*/

int main(int argc, char **argv) {
  if (argc<3) {
    printf("Usage: %s <server_address> <player texture>\n", argv[0]);
    exit(-1);
  }

    // Casting arguments
  char* server_ip = get_hostname_from_arg(argv[1]);
  int server_port = get_portno_from_arg(argv[1]);
  char* player_texture_path = argv[2];
  if (server_ip == NULL || server_port == -1 )
    print_err("Error while reading input arguments. Check <Server_address>.\n");

    // Loading player's texture
  printf("loading texture image from %s ... ", player_texture_path);
  Image* player_image = Image_load(player_texture_path);
  if (player_image) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
    
    // Profile texture = player texture (the blue arrow)
  Image* profile_image = Image_load(player_texture_path);
  if (profile_image) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

    // Connecting to server to exchange information
  int my_id;
	Image* my_texture_from_server;
	Image* map_texture;
  Image* map_elevation;
    
  int sockfd;
  struct sockaddr_in server_addr = {0};

    // Create TCP Socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    print_err("Error while opening socket\n");

    // Setting server address
  server_addr.sin_addr.s_addr = inet_addr(server_ip);
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(server_port); // don't forget about network byte order!

    // Connecting to server_addr
  if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
    print_err("Error while connecting to server\n");

    // INFO: send your image profile and get (id, surface texture, elevation texture)
  char data[INCOMING_DATA_SIZE];
	int data_len;
	
    // Build IdPacket to ask an ID from Server
  IdPacket* request_id_packet = malloc(sizeof(IdPacket));
  PacketHeader id_header;
  id_header.type = GetId;
  request_id_packet->header = id_header;
  request_id_packet->id = -1;

    // Send deserialized IdPacket
	data_len = Packet_serialize(data, &request_id_packet->header);
	sendPacket(sockfd, data, data_len);
	Packet_free((PacketHeader *) request_id_packet);

	printf("Asking for an ID to server...\n");

    // Receive serialized IdPacket containing my new ID
	data_len = receivePacket(sockfd, data);
	
  IdPacket* received_id_packet = (IdPacket*) Packet_deserialize(data, data_len);
  my_id = received_id_packet->id;
  Packet_free((PacketHeader *) received_id_packet);

  printf(" ***** ID assigned from server: %d *****\n", my_id);
	
		// Build ImagePacket to send profile's texture
  ImagePacket* profile_texture_packet = malloc(sizeof(ImagePacket));
		  PacketHeader profile_texture_packet_header;
		  profile_texture_packet_header.type = PostTexture;
  profile_texture_packet->header = profile_texture_packet_header;
  profile_texture_packet->id = my_id;
	profile_texture_packet->image = profile_image;
	
    // Send serialized ImagePacket containing image profile
  data_len = Packet_serialize(data, &profile_texture_packet->header);
  sendPacket(sockfd, data, data_len);
  Packet_free((PacketHeader *) profile_texture_packet);

	printf("Your profile texture has been sent to server.\nWaiting for agreement..");
	
    // Receive serialized ImagePacket containing the server copy of image profile 
	data_len = receivePacket(sockfd, data);
  ImagePacket* received_profile_texture_packet = (ImagePacket*) Packet_deserialize(data, data_len);
  my_texture_from_server = received_profile_texture_packet->image;
  
		// Don't free this packet to keep in memory the image 
	// Packet_free((PacketHeader *) received_profile_texture_packet);
	
	printf("OK.\n");
	
	/*
	
    // Receive serialized ImagePacket containing the texture map 
	data_len = INCOMING_DATA_SIZE;
	bzero(data, data_len);
	data_received = recv(sockfd, data, data_len, 0);
  //ImagePacket* received_texture_map_packet = (ImagePacket*) Packet_deserialize(data, data_received);
  //map_texture = received_texture_map_packet->image;
  //Packet_free((PacketHeader *) received_texture_map_packet);
	printf("%d bytes received\n", data_received);
	printf("Received texture map from server.\n");
	
    // Receive serialized ImagePacket containing the elevation map
	data_len = INCOMING_DATA_SIZE;
	bzero(data, data_len);
	data_received = recv(sockfd, data, data_len, 0);
  //ImagePacket* received_elevation_map_packet = (ImagePacket*) Packet_deserialize(data, data_received);
  //map_elevation = received_elevation_map_packet->image;
  //Packet_free((PacketHeader *) received_elevation_map_packet);
	printf("%d bytes received\n", data_received);
	printf("Received elevation map from server.\n");
	*/
    // Closing TCP socket
  if (close(sockfd) < 0)
    print_err("Error while closing socket\n");

  exit(0);

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  vehicle = (Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
  World_addVehicle(&world, vehicle);

  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;             
}
