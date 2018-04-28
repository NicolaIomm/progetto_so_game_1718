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

  int ret;

    // Casting arguments
  char* server_ip = get_hostname_from_arg(argv[1]);
  int server_port = get_portno_from_arg(argv[1]);
  char* player_texture_path = argv[2];
  if (server_ip == NULL || server_port == -1 )
    print_err("Error while reading input arguments. Check <Server_address>.\n");

    // Loading player's texture
  printf("\t\t ***** Loading texture image from %s ... ", player_texture_path);
  Image* player_image = Image_load(player_texture_path);
  if (player_image) {
    printf("Done! *****\n\n");
  } else {
    printf("Fail! *****\n\n");
  }

    // Loading profile texture		// CHANGE THIS !!!
	printf("\t\t ***** Loading profile texture image from %s ... ", player_texture_path);
  Image* profile_image = Image_load(player_texture_path);
  if (profile_image) {
    printf("Done! *****\n\n");
  } else {
    printf("Fail! *****\n\n");
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
  IdPacket* request_id_packet = (IdPacket*) malloc(sizeof(IdPacket));
    PacketHeader id_header;
    id_header.type = GetId;
  request_id_packet->header = id_header;
  request_id_packet->id = -1;

    // Send deserialized IdPacket
	data_len = Packet_serialize(data, &request_id_packet->header);
	sendPacketTCP(sockfd, data, data_len);
	Packet_free((PacketHeader *) request_id_packet);

	printf("Asking for an ID to server...\n");

    // Receive serialized IdPacket containing my new ID
	data_len = receivePacketTCP(sockfd, data);

  IdPacket* received_id_packet = (IdPacket*) Packet_deserialize(data, data_len);
  my_id = received_id_packet->id;
  Packet_free((PacketHeader *) received_id_packet);

  printf("\t\t ***** ID assigned from server: %d *****\n\n", my_id);

		// Build ImagePacket to send profile's texture
  ImagePacket* profile_texture_packet = (ImagePacket*) malloc(sizeof(ImagePacket));
		  PacketHeader profile_texture_packet_header;
		  profile_texture_packet_header.type = PostTexture;
  profile_texture_packet->header = profile_texture_packet_header;
  profile_texture_packet->id = my_id;
	profile_texture_packet->image = profile_image;

    // Send serialized ImagePacket containing image profile
  data_len = Packet_serialize(data, &profile_texture_packet->header);
  sendPacketTCP(sockfd, data, data_len);
  Packet_free((PacketHeader *) profile_texture_packet);

	printf("Your profile texture has been sent to server. Waiting for agreement..\n");

    // Receive serialized ImagePacket containing the server copy of image profile
	data_len = receivePacketTCP(sockfd, data);
  ImagePacket* received_profile_texture_packet = (ImagePacket*) Packet_deserialize(data, data_len);
  my_texture_from_server = received_profile_texture_packet->image;
	Packet_free((PacketHeader *) received_profile_texture_packet);

	printf("\t\t ***** Server agreed your profile texture! *****\n\n");
/*
		// Build ImagePacket asking texture_surface to server
	ImagePacket* request_texture_surface = (ImagePacket*) malloc(sizeof(ImagePacket));
		PacketHeader request_texture_surface_header;
		request_texture_surface_header.type = GetTexture;
	request_texture_surface->header = request_texture_surface_header;
	request_texture_surface->id = my_id;
	request_texture_surface->image = NULL;

    // Send serialized ImagePacket containing the request for texture_surface
  data_len = Packet_serialize(data, &request_texture_surface->header);
  sendPacketTCP(sockfd, data, data_len);
  Packet_free((PacketHeader *) request_texture_surface);

	printf("Asking for the texture_surface to server...\n");

    // Receive serialized ImagePacket containing the texture_surface
	data_len = receivePacketTCP(sockfd, data);
  ImagePacket* texture_surface_packet = (ImagePacket*) Packet_deserialize(data, data_len);
  map_texture = texture_surface_packet->image;
	Packet_free((PacketHeader *) texture_surface_packet);

	printf("\t\t ***** Texture surface received successfully from server! *****\n\n");

		// Build ImagePacket asking texture_elevation to server
	ImagePacket* request_texture_elevation = (ImagePacket*) malloc(sizeof(ImagePacket));
		PacketHeader request_texture_elevation_header;
		request_texture_elevation_header.type = GetElevation;
	request_texture_elevation->header = request_texture_elevation_header;
	request_texture_elevation->id = my_id;
	request_texture_elevation->image = NULL;

    // Send serialized ImagePacket containing the request for texture_elevation
  data_len = Packet_serialize(data, &request_texture_elevation->header);
  sendPacketTCP(sockfd, data, data_len);
  Packet_free((PacketHeader *) request_texture_elevation);

	printf("Asking for the texture_elevation to server...\n");

    // Receive serialized ImagePacket containing the texture_elevation
	data_len = receivePacketTCP(sockfd, data);
        ImagePacket* texture_elevation_packet = (ImagePacket*) Packet_deserialize(data, data_len);
        map_elevation = texture_elevation_packet->image;
	Packet_free((PacketHeader *) texture_elevation_packet);

	printf("\t\t ***** Texture elevation received succesfully from server! *****\n\n");
*/

    map_elevation = Image_load("./images/maze.pgm");
    if (map_elevation) {
    printf("Done! \n");
    } else {
    printf("Fail! \n");
    }

    map_texture = Image_load("./images/depth1.pgm");
    if (map_texture) {
    printf("Done! \n");
    } else {
    printf("Fail! \n");
    }

    // Closing TCP socket
    ret = close(sockfd);
    if (ret < 0)
        print_err("Error while closing socket\n");

	printf("\t\t ***** Closing TCP Connection! *****\n\n");

    // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);

    // construct and init my vehicle
  vehicle = (Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, my_texture_from_server);

  		// Build ClientUpdate packet to send my vehicle
  ClientUpdate* my_vehicle_packet = (ClientUpdate*) malloc(sizeof(ClientUpdate));
		  PacketHeader my_vehicle_packet_header;
		  my_vehicle_packet_header.type = VechicleUpdate;
  my_vehicle_packet->header = my_vehicle_packet_header;
  my_vehicle_packet->id = my_id;
  my_vehicle_packet->x = vehicle->x;
  my_vehicle_packet->y = vehicle->y;
  my_vehicle_packet->theta = vehicle->theta;

        // Send ClientUpdate packet to UDP Server
  data_len = Packet_serialize(data, &my_vehicle_packet->header);
  sendPacketUDP(sockfd, data, data_len);
  Packet_free((PacketHeader *) my_vehicle_packet);

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
