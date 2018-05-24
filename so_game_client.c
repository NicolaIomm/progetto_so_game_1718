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
#include "world_viewer.h"
#include "vehicle.h"
#include <pthread.h>

#include <sys/socket.h> // socket(), connect()
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h>  // hton() , inet_addr()

#include "so_game_protocol.h"
#include "stream_socket.h"

#include <sys/types.h>
#include <signal.h>

#define DEBUG 0

#define TIME_TO_USLEEP_SENDER   30000  // 30 ms

#define INCOMING_DATA_SIZE 2000000

void sighup_signal_handler(int signal);

  // Socket (TCP)
int sock_tcp;

  // Socket and server_addr (UDP)
int sock_udp;
struct sockaddr_in server_addr;
socklen_t server_len = sizeof(server_addr);

  // Global variables for the game
World world;
Vehicle* vehicle;

void* listener_update_thread_handler_UDP(void* arg_null){

  printf("Listener thread started.\n");

  char UDP_buff[INCOMING_DATA_SIZE];
  bzero(UDP_buff, INCOMING_DATA_SIZE);

  int ret;
  int bytes_received;
  int bytes_sent;

  while (1){

        // Receiving UDP Packet
      bytes_received = receivePacketUDP(sock_udp, UDP_buff, (sockaddr *)&server_addr, &server_len);
      if (bytes_received < 0)
        print_err("Error while receiving an UDP packet.");

        // Determine the type of Packet
      PacketHeader* general_packet = (PacketHeader*) Packet_deserialize(UDP_buff, bytes_received);

      if (DEBUG)
        printf("Packet type: %d\nPacket size:%d\n\n", ((PacketHeader *) UDP_buff)->type, ((PacketHeader *) UDP_buff)->size);

      if (general_packet->type == Disconnection){
        IdPacket* disconnection = (IdPacket*) general_packet;
        int target_id = disconnection->id;

        Vehicle* target_vehicle = World_getVehicle(&world, target_id);

        World_detachVehicle(&world, target_vehicle);
        free(target_vehicle->texture);
        free(target_vehicle);

      }
      else if (general_packet->type == WorldUpdate){

        WorldUpdatePacket* world_update_packet = (WorldUpdatePacket*) general_packet;
          int n = world_update_packet->num_vehicles;
          ClientUpdate* updates_vec = world_update_packet->updates;
        Packet_free((PacketHeader *) world_update_packet);
        bzero(UDP_buff, bytes_received);

        if (DEBUG)
          printf("Pacchetto ricevuto con %d client_update !\n", n);

          // Update all vehicles in the world
        int i;
        for (i = 0; i < n; i++){
            int current_vehicle_id = updates_vec[i].id;

            if (DEBUG && current_vehicle_id == vehicle->id)
              continue;

              // Get Vehicle with id from my copy of world
            Vehicle* current_vehicle = World_getVehicle(&world, current_vehicle_id);
            if (current_vehicle == NULL){
                Vehicle* new_vehicle = (Vehicle*) malloc(sizeof(Vehicle));

              if (DEBUG)
                printf("Richiedo la texture per il veicolo %d\n", current_vehicle_id);

                  // Build ImagePacket to ask the vehicle texture with id = current_vehicle_id
              ImagePacket* new_vehicle_texture = (ImagePacket*) malloc(sizeof(ImagePacket));
                PacketHeader new_vehicle_texture_header;
                new_vehicle_texture_header.type = GetTexture;
              new_vehicle_texture->header = new_vehicle_texture_header;
              new_vehicle_texture->id = current_vehicle_id;
              new_vehicle_texture->image = NULL;

              // Send serialized ImagePacket containing image profile
              bzero(UDP_buff, bytes_received);
              bytes_sent = Packet_serialize(UDP_buff, &new_vehicle_texture->header);
              sendPacketUDP(sock_udp, UDP_buff, bytes_sent, (sockaddr *)&server_addr, server_len);
              Packet_free((PacketHeader *) new_vehicle_texture);

                // Wait until texture of the vehicle is loaded
                // Receive serialized ImagePacket containing the new
              bzero(UDP_buff, bytes_sent);
              bytes_received = receivePacketTCP(sock_tcp, UDP_buff);

              ImagePacket* received_texture = (ImagePacket*) Packet_deserialize(UDP_buff, bytes_received);
              int id = received_texture->id;
              Image* image = received_texture->image;

              if (DEBUG)
                printf("Ho ricevuto la texture del veicolo %d\n", id);

              Packet_free((PacketHeader *) received_texture);

              if (DEBUG)
                printf("Packet type: %d\nPacket size:%d\n\n", ((PacketHeader *) UDP_buff)->type, ((PacketHeader *) UDP_buff)->size);

              if (DEBUG){
                char name[20];
                printf("Ho ricevuto la texture del veicolo %d.\n", id);
                sprintf(name, "inClient%d.pgm", id);
                Image_save(image, name);
              }

              bzero(UDP_buff, bytes_received);

              Vehicle_init(new_vehicle, &world, current_vehicle_id, image);

              World_addVehicle(&world, new_vehicle);
              current_vehicle = World_getVehicle(&world, current_vehicle_id);
            }
            else {

              ClientUpdate* current_update = &updates_vec[i];
              current_vehicle->x = current_update->x;
              current_vehicle->y = current_update->y;
              current_vehicle->theta = current_update->theta;
            }
        }

        // Update the world including all vehicles position
      World_update(&world);
    }

    Packet_free((PacketHeader*) general_packet);
  }

  return NULL;
}

void* sender_update_thread_handler_UDP(void* arg_null){

  printf("Sender thread started.\n");

  char UDP_buff[INCOMING_DATA_SIZE];

  int ret;
  int bytes_sent;

  while (1){

        // Build VehicleUpdatePacket packet to send my vehicle
      VehicleUpdatePacket* my_vehicle_packet = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));
          PacketHeader my_vehicle_packet_header;
          my_vehicle_packet_header.type = VehicleUpdate;
      my_vehicle_packet->header = my_vehicle_packet_header;
      my_vehicle_packet->id = vehicle->id;
      my_vehicle_packet->rotational_force = vehicle->rotational_force_update;
      my_vehicle_packet->translational_force = vehicle->translational_force_update;

            // Send VehicleUpdatePacket packet to UDP Server
      bytes_sent = Packet_serialize(UDP_buff, &my_vehicle_packet->header);
      sendPacketUDP(sock_udp, UDP_buff, bytes_sent, (sockaddr *)&server_addr, server_len);

      if (DEBUG){
        printf("Pacchetto inviato al server\n");
        printf("Rotational_force = %f\n", my_vehicle_packet->rotational_force);
        printf("Translational_force = %f\n", my_vehicle_packet->translational_force);
        printf("UDP_buff: %s\n", UDP_buff);
      }

      Packet_free((PacketHeader *) my_vehicle_packet);
      bzero(UDP_buff, bytes_sent);

      ret = usleep(TIME_TO_USLEEP_SENDER);
      if (ret < 0)
        print_err("Impossible to sleep the listener_update_thread_handler_UDP.\n");

  }
  return NULL;
}

void* texture_thread_handler(void* arg_null){

  char data[INCOMING_DATA_SIZE];
  int data_len;

  int wait = 1;
  while(wait){
        // Receive serialized ImagePacket containing the new
      data_len = receivePacketTCP(sock_tcp, data);

      ImagePacket* received_texture = (ImagePacket*) Packet_deserialize(data, data_len);
      int id = received_texture->id;
      Image* image = received_texture->image;

      Vehicle* target_vehicle = World_getVehicle(&world, id);
      target_vehicle->texture = image;

      Packet_free((PacketHeader *) received_texture);

      if (DEBUG)
        printf("Packet type: %d\nPacket size:%d\n\n", ((PacketHeader *) data)->type, ((PacketHeader *) data)->size);

      if (DEBUG){
        char name[20];
        sprintf(name, "inClient%d.pgm", id);
        Image_save(image, name);
      }

      bzero(data, data_len);

      // Wait 1 sec
    usleep(1000000);
  }
}

int main(int argc, char **argv) {
  if (argc<3) {
    printf("Usage: %s <server_address> <player texture>\n", argv[0]);
    exit(-1);
  }

    // buffer to comunicate with server (TCP or UDP)
  char data[INCOMING_DATA_SIZE];
  int data_len;

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

    // Create TCP Socket
  sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_tcp < 0)
    print_err("Error while opening socket\n");

    // Setting server address
  server_addr.sin_addr.s_addr = inet_addr(server_ip);
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(server_port); // don't forget about network byte order!

    // Connecting to server_addr
  if (connect(sock_tcp, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
    print_err("Error while connecting to server\n");

    // Build IdPacket to ask an ID from Server
  IdPacket* request_id_packet = (IdPacket*) malloc(sizeof(IdPacket));
    PacketHeader id_header;
    id_header.type = GetId;
  request_id_packet->header = id_header;
  request_id_packet->id = -1;

    // Send deserialized IdPacket
	data_len = Packet_serialize(data, &request_id_packet->header);
	sendPacketTCP(sock_tcp, data, data_len);
	Packet_free((PacketHeader *) request_id_packet);

	printf("Asking for an ID to server...\n");

    // Receive serialized IdPacket containing my new ID
	data_len = receivePacketTCP(sock_tcp, data);

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
  sendPacketTCP(sock_tcp, data, data_len);
  Packet_free((PacketHeader *) profile_texture_packet);

	printf("Your profile texture has been sent to server. Waiting for agreement..\n");

    // Receive serialized ImagePacket containing the server copy of image profile
	data_len = receivePacketTCP(sock_tcp, data);
  ImagePacket* received_profile_texture_packet = (ImagePacket*) Packet_deserialize(data, data_len);
  my_texture_from_server = received_profile_texture_packet->image;
	Packet_free((PacketHeader *) received_profile_texture_packet);

	printf("\t\t ***** Server agreed your profile texture! *****\n\n");

		// Build ImagePacket asking texture_surface to server
	ImagePacket* request_texture_surface = (ImagePacket*) malloc(sizeof(ImagePacket));
		PacketHeader request_texture_surface_header;
		request_texture_surface_header.type = GetTexture;
	request_texture_surface->header = request_texture_surface_header;
	request_texture_surface->id = my_id;
	request_texture_surface->image = NULL;

    // Send serialized ImagePacket containing the request for texture_surface
  data_len = Packet_serialize(data, &request_texture_surface->header);
  sendPacketTCP(sock_tcp, data, data_len);
  Packet_free((PacketHeader *) request_texture_surface);

	printf("Asking for the texture_surface to server...\n");

    // Receive serialized ImagePacket containing the texture_surface
	data_len = receivePacketTCP(sock_tcp, data);
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
  sendPacketTCP(sock_tcp, data, data_len);
  Packet_free((PacketHeader *) request_texture_elevation);

	printf("Asking for the texture_elevation to server...\n");

    // Receive serialized ImagePacket containing the texture_elevation
	data_len = receivePacketTCP(sock_tcp, data);
        ImagePacket* texture_elevation_packet = (ImagePacket*) Packet_deserialize(data, data_len);
        map_elevation = texture_elevation_packet->image;
	Packet_free((PacketHeader *) texture_elevation_packet);

	printf("\t\t ***** Texture elevation received succesfully from server! *****\n\n");

    // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);

        // Initializing UDP connection (server_ip is the same and server_port is the next port of the TCP port)
  sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
  server_port += 1;
  server_addr = {0};

  if (sock_udp < 0)
    print_err("Error while opening UDP socket\n");

  server_addr.sin_addr.s_addr = inet_addr(server_ip);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);

    // construct and init my vehicle
  vehicle = (Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, my_texture_from_server);

  World_addVehicle(&world, vehicle);

  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool

  pthread_t listener_update_thread;
  ret = pthread_create(&listener_update_thread, NULL, listener_update_thread_handler_UDP, NULL);
  if (ret != 0)
    print_err("Cannot create the update_handler_UDP thread");
  printf("Started new listener_update_thread (UDP connection).\n");

  ret = pthread_detach(listener_update_thread);
  if (ret != 0)
    print_err("Cannot detach the update_handler_UDP thread");

  pthread_t sender_update_thread;
  ret = pthread_create(&sender_update_thread, NULL, sender_update_thread_handler_UDP, NULL);
  if (ret != 0)
    print_err("Cannot create the update_handler_UDP thread");
  printf("Started new sender_update_thread (UDP connection).\n");

  ret = pthread_detach(sender_update_thread);
  if (ret != 0)
    print_err("Cannot detach the update_handler_UDP thread");

    // Set up sighup handler
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sighup_signal_handler;

  ret = sigaction(SIGHUP, &sa, NULL);
  if (ret == -1)
    print_err("Cannot set up SIGUP signal handler\n");

  if (DEBUG)
    printf("Signal handler installed successfully\n");

    // Start the world viewer from your vehicle point of view
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  return 0;
}

void sighup_signal_handler(int signal){

  int ret;
  int buff_size = 100;
  char* tmp_buff = (char*) malloc(buff_size);
  int bytes_sent;

    // Closing TCP socket
  ret = close(sock_tcp);
  if (ret < 0)
    print_err("Error while closing TCP socket\n");

	printf("\t\t ***** Closed TCP Connection! *****\n\n");

    // Sending the DisconnectionPacket to server
  IdPacket* disconnection = (IdPacket*) malloc(sizeof(IdPacket));
		  PacketHeader disconnection_header;
		  disconnection_header.type = Disconnection;
  disconnection->header = disconnection_header;
  disconnection->id = vehicle->id;

  bzero(tmp_buff, buff_size);
  bytes_sent = Packet_serialize(tmp_buff, &disconnection->header);
  sendPacketUDP(sock_udp, tmp_buff, bytes_sent, (sockaddr *)&server_addr, server_len);

  if (DEBUG)
    printf("Packet type: %d\nPacket size:%d\n\n", ((PacketHeader *) tmp_buff)->type, ((PacketHeader *) tmp_buff)->size);

  Packet_free((PacketHeader *) disconnection);

  free(tmp_buff);

    // Destroy the world
  World_destroy(&world);

	printf("\t\t ***** BYE BYE! *****\n\n");

  exit(0);

}
