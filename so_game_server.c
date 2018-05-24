// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include "so_game_protocol.h"

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "stream_socket.h"

#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#define DEBUG 0

#define SERVER_PORT_TCP 3000
#define SERVER_PORT_UDP 3001

#define TIME_TO_USLEEP_SENDER   30000 // 30 ms

#define INCOMING_DATA_SIZE 2000000

int global_id=1;

int sock_udp;                // UDP info
struct sockaddr_in my_addr = {0};

World world;

ListHead client_list;

typedef struct ClientVehicle{
  ListItem list;
  int id;
  struct sockaddr* ip;
  int sock_tcp;
} ClientVehicle;

/*
void* prova_func(void* arg_null){

  while (1){
    printf("\t--> Client connessi: %d\n", client_list.size);
    usleep(2000000);
  }
}
*/

void* listener_update_thread_handler_UDP(void* arg_null){
  // This thread will handle all incoming UDP connection coming from client

  printf("Listener thread started.\n");

  char UDP_buff[INCOMING_DATA_SIZE];
  bzero(UDP_buff, INCOMING_DATA_SIZE);

  int ret;
  int bytes_received;
  int bytes_sent;

  while (1){

    socklen_t client_addr_len = sizeof(struct sockaddr);
    struct sockaddr* client_addr = (struct sockaddr*)malloc(client_addr_len);

      // Receiving UDP Packet
    bytes_received = receivePacketUDP(sock_udp, UDP_buff, client_addr, &client_addr_len);
    if (bytes_received < 0)
      print_err("Error while receiving an UDP packet.");

      // Determine the type of Packet
    PacketHeader* general_packet = (PacketHeader*) Packet_deserialize(UDP_buff, bytes_received);

    if (general_packet->type == Disconnection){
      IdPacket* disconnection = (IdPacket*) general_packet;
      int target_id = disconnection->id;

      if (DEBUG)
        printf("Packet type: %d\nPacket size:%d\n\n", ((PacketHeader *) UDP_buff)->type, ((PacketHeader *) UDP_buff)->size);

        // Build IdPacket to notify the disconnection of client id
      IdPacket* notify_disconnection = (IdPacket*) malloc(sizeof(IdPacket));
        PacketHeader notify_disconnection_header;
        notify_disconnection_header.type = Disconnection;
      notify_disconnection->header = notify_disconnection_header;
      notify_disconnection->id = target_id;

      bytes_sent = Packet_serialize(UDP_buff, &notify_disconnection->header);
      ListItem* iterator;

        // Remove ClientVehicle from the ClientList
      ListItem* target_client = NULL;
      iterator = client_list.first;
        while (iterator != NULL && target_client == NULL){
          ClientVehicle* current = (ClientVehicle*) iterator;
          if (current->id == target_id){
            target_client = iterator;
          }
          iterator = iterator->next;
        }

      List_detach(&client_list, target_client);
      free(((ClientVehicle*)target_client)->ip);
      close(((ClientVehicle*)target_client)->sock_tcp);
      free(((ClientVehicle*)target_client));

        // Send disconnectionPacket to all clients in ClientList
      iterator = client_list.first;
      while (iterator != NULL){
        ClientVehicle* current = (ClientVehicle*) iterator;
        sendPacketUDP(sock_udp, UDP_buff, bytes_sent, current->ip, client_addr_len);
        iterator = iterator->next;
      }

      Packet_free((PacketHeader *) notify_disconnection);

      bzero(UDP_buff, bytes_received);

        // Remove target Vehicle from the world
      Vehicle* target_vehicle = World_getVehicle(&world, target_id);
      World_detachVehicle(&world, target_vehicle);

      printf("*** Veicolo %d disconnesso.\n", target_vehicle->id);

      //Image_free(target_vehicle->texture); //DO NOT INCLUDE THIS
      Vehicle_destroy(target_vehicle);

    }
    else if (general_packet->type == GetTexture){
        ImagePacket* image_request = (ImagePacket*) general_packet;
        int id = image_request->id;

        if (DEBUG){
          printf("Un client ha richiesto la texture del veicolo %d\n", id);
        }

        ListItem* iterator = client_list.first;
        while (iterator != NULL){
          ClientVehicle* current = (ClientVehicle*) iterator;
          if (memcmp(current->ip, client_addr, client_addr_len) == 0) {

            int target_socket = current->sock_tcp;
            Vehicle* target_vehicle = World_getVehicle(&world, id);
            Image* target_texture = target_vehicle->texture;

              // Build ImagePacket to send the texture to client
            ImagePacket* texture = (ImagePacket*) malloc(sizeof(ImagePacket));
              PacketHeader texture_header;
              texture_header.type = PostTexture;
            texture->header = texture_header;
            texture->id = id;
            texture->image = target_texture;

              // Send deserialized ImagePacket
            bzero(UDP_buff, bytes_received);
            bytes_sent = Packet_serialize(UDP_buff, &texture->header);
            sendPacketTCP(target_socket, UDP_buff, bytes_sent);
            Packet_free((PacketHeader *) texture);

            if (DEBUG)
              printf("Sent to sock: %d\nPacket type: %d\nPacket size:%d\n\n", target_socket, ((PacketHeader *) UDP_buff)->type, ((PacketHeader *) UDP_buff)->size);

            break;
          }
          iterator = iterator->next;
        }
        bzero(UDP_buff, bytes_sent);
    }
    else if (general_packet->type == VehicleUpdate){
        // Analyzing VehicleUpdatePacket
      VehicleUpdatePacket* vehicle_update = (VehicleUpdatePacket*) general_packet;
      int id = vehicle_update->id;
      float rotational_force = vehicle_update->rotational_force;
      float translational_force = vehicle_update->translational_force;

      if (DEBUG){
        printf("Pacchetto ricevuto dal veicolo %d\n", id);
        printf("Rotational_force = %f\n", rotational_force);
        printf("Translational_force = %f\n", translational_force);
        printf("UDP_buff: %s\n", UDP_buff);
      }

      bzero(UDP_buff, bytes_received);

        // Store address_ip (for UDP) in client_list if there isn't
      ListItem* iterator = client_list.first;
      int thereIs = 0;
      while (iterator != NULL){
        ClientVehicle* client_vehicle = (ClientVehicle*) iterator;
        if (client_vehicle->id == id && client_vehicle->ip == NULL)
          client_vehicle->ip = client_addr;
        iterator = iterator->next;
      }

        // Update vehicle
      Vehicle* current_vehicle = World_getVehicle(&world, id);
      if (current_vehicle == NULL){
        if (DEBUG)
          printf("ERROR: Veicolo ID:%d non presente nel mondo !!!\n", id);
        continue;
      }
      current_vehicle->rotational_force_update = rotational_force;
      current_vehicle->translational_force_update = translational_force;

        // Update_world
      World_update(&world);
    }

    Packet_free((PacketHeader*) general_packet);

  }
  return NULL;
}

void* sender_update_thread_handler_UDP(void* arg_null){
  // This thread will send the WorldUpdatePacket to all client in game

  printf("Sender thread started.\n");

  char UDP_buff[INCOMING_DATA_SIZE];

  while (1){
    int ret;
    int bytes_sent;

    ListItem* iterator;

      // Build ClientUpdate array
    ClientUpdate* updates = (ClientUpdate*) malloc(client_list.size * sizeof(ClientUpdate));

    int i = 0;
    iterator = client_list.first;
    while (iterator != NULL){

      Vehicle* vehicle = World_getVehicle(&world, ((ClientVehicle*)iterator)->id);

      updates[i].id = vehicle->id;
      updates[i].x = vehicle->x;
      updates[i].y = vehicle->y;
      updates[i].theta = vehicle->theta;

      if (DEBUG){
        printf("AGGIORNAMENTI INVIATI:\n");
        printf("\tid:%d\n",updates[i].id);
        printf("\tx:%f\n",updates[i].x);
        printf("\ty:%f\n",updates[i].y);
        printf("\ttheta:%f\n",updates[i].theta);
      }

      i++;
      iterator = iterator->next;
    }

      // Build WorldUpdatePacket
    WorldUpdatePacket* worldUpdatePacket = (WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
      PacketHeader worldUpdatePacket_header;
      worldUpdatePacket_header.type = WorldUpdate;
    worldUpdatePacket->header = worldUpdatePacket_header;
    worldUpdatePacket->num_vehicles = i;
    worldUpdatePacket->updates = updates;

      // Serialize WorldPacketUpdate
    bytes_sent = Packet_serialize(UDP_buff, &(worldUpdatePacket->header));

    if (DEBUG)
      printf("Packet type: %d\nPacket size:%d\n\n", ((PacketHeader *) UDP_buff)->type, ((PacketHeader *) UDP_buff)->size);

      // Iterate on all ClientVehicle connected
    iterator = client_list.first;
    while (iterator != NULL){

      ClientVehicle* client_vehicle = (ClientVehicle*)iterator;
        // Send WorldUpdatePacket just serialized in UDP_buff to them
      sendPacketUDP(sock_udp, UDP_buff, bytes_sent, client_vehicle->ip, (socklen_t)sizeof(struct sockaddr_in));
      iterator = iterator->next;
    }

      // Clear UDP_buff
    bzero(UDP_buff, bytes_sent);

      // Sleep the thread
    ret = usleep(TIME_TO_USLEEP_SENDER);
    if (ret < 0)
      print_err("Impossible to usleep listener_thread_handler_UDP.\n");
  }

  return NULL;
}

typedef struct thread_arg_TCP{
 Image* surface_texture;
 Image* elevation_texture;
 int socket;
} Thread_arg_TCP;

void* thread_handler_TCP(void* arg){
    Thread_arg_TCP* tr=(Thread_arg_TCP*)arg;
    Image* srf_text=tr->surface_texture;
    Image* elv_text=tr->elevation_texture;
    int socket_desc=tr->socket;
    int ret;

    char data[INCOMING_DATA_SIZE];
    int data_len;

    data_len = receivePacketTCP(socket_desc, data);
    IdPacket* received_id_packet = (IdPacket*) Packet_deserialize(data, data_len);

    if (DEBUG)
      printf("Received requestIDPacket\n");

    int id_received = received_id_packet->id;
    int id_assigned;
    if (id_received == -1){
            // Build IdPacket to ask an ID from Server
        IdPacket* request_id_packet = (IdPacket*)malloc(sizeof(IdPacket));
        PacketHeader id_header;
        id_header.type = GetId;
        request_id_packet->header = id_header;
        request_id_packet->id = global_id;
        id_assigned = global_id;

        global_id++;

          // Sent deserialized IdPacket
        data_len = Packet_serialize(data, &request_id_packet->header);
        sendPacketTCP(socket_desc, data, data_len);
        Packet_free((PacketHeader*) request_id_packet);
    }

    Packet_free((PacketHeader*) received_id_packet);

    if (DEBUG)
     printf("Sent a new IDPacket to client\n");

      // Receive the player's texture from client
    data_len = receivePacketTCP(socket_desc, data);
    ImagePacket* received_profile_texture = (ImagePacket*) Packet_deserialize(data, data_len);

    Image* profile_texture_image = received_profile_texture->image;

    Packet_free((PacketHeader*) received_profile_texture);

    if (DEBUG)
     printf("Received profile texture from client\n");

      // Resend the player's texture to client to confirm the texture
    ImagePacket* confirmed_player_texture = (ImagePacket*)malloc(sizeof(ImagePacket));
    PacketHeader confirmed_player_texture_header;
    confirmed_player_texture_header.type = PostTexture;
    confirmed_player_texture->header = confirmed_player_texture_header;
    confirmed_player_texture->id = global_id;
    confirmed_player_texture->image = profile_texture_image;

    data_len = Packet_serialize(data, &confirmed_player_texture->header);
    sendPacketTCP(socket_desc, data, data_len);

    Packet_free((PacketHeader*) confirmed_player_texture);

      // Receive the request for surface texture
    data_len = receivePacketTCP(socket_desc, data);
    ImagePacket* texture_surface_packet_request = (ImagePacket*) Packet_deserialize(data, data_len);
       // Send surface texture to client
      ImagePacket* texture_surface_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
      PacketHeader texture_surface_packet_header;
      texture_surface_packet_header.type = PostTexture;
      texture_surface_packet->header = texture_surface_packet_header;
      texture_surface_packet->id = 0;
      texture_surface_packet->image = srf_text;

      data_len = Packet_serialize(data, &texture_surface_packet->header);
      sendPacketTCP(socket_desc, data, data_len);
      Packet_free((PacketHeader*) texture_surface_packet);
    Packet_free((PacketHeader *) texture_surface_packet_request);

      // Receive the request for elevation texture
    data_len = receivePacketTCP(socket_desc, data);
    ImagePacket* texture_elevation_packet_request = (ImagePacket*) Packet_deserialize(data, data_len);
       // Send elevation texture to client
      ImagePacket* texture_elevation_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
      PacketHeader texture_elevation_packet_header;
      texture_elevation_packet_header.type = PostTexture;
      texture_elevation_packet->header = texture_elevation_packet_header;
      texture_elevation_packet->id = 0;
      texture_elevation_packet->image = elv_text;

      data_len = Packet_serialize(data, &texture_elevation_packet->header);
      sendPacketTCP(socket_desc, data, data_len);
      Packet_free((PacketHeader*) texture_elevation_packet);
    Packet_free((PacketHeader *) texture_elevation_packet_request);

      // Add new Vehicle to world
    Vehicle* new_vehicle = (Vehicle*) malloc(sizeof(Vehicle));
    Vehicle_init(new_vehicle, &world, id_assigned, profile_texture_image);
    World_addVehicle(&world, new_vehicle);

    if (DEBUG){
      printf("Veicolo con ID=%d aggiunto al mondo.\n", id_assigned);
    }

      // Add new ClientVehicle to client_list
    ClientVehicle* new_client = (ClientVehicle*) malloc(sizeof(ClientVehicle));
    new_client->id = id_assigned;
    new_client->sock_tcp = socket_desc;

    List_insert(&client_list, client_list.last, (ListItem*)new_client);
    if (DEBUG)
      printf("Actual clients: %d\n",client_list.size);

    printf("*** Veicolo %d connesso.\n", new_client->id);

    return NULL;
}

int main(int argc, char **argv) {
  if (argc<3) {
    printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
    exit(-1);
  }
  char* elevation_filename=argv[1];
  char* texture_filename=argv[2];
  char* vehicle_texture_filename="./images/arrow-right.ppm";
  printf("loading elevation image from %s ... ", elevation_filename);

  // load the images
  Image* surface_elevation = Image_load(elevation_filename);
  if (surface_elevation) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  printf("loading texture image from %s ... ", texture_filename);
  Image* surface_texture = Image_load(texture_filename);
  if (surface_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  /*
      Contructing World
                          */
  World_init(&world, surface_elevation, surface_texture, 0.5, 0.5, 0.5);

  List_init(&client_list);

  /*
    starting server UDP
                        */
    int ret;

    sock_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    my_addr.sin_family=AF_INET;
    my_addr.sin_port=htons(SERVER_PORT_UDP);
    my_addr.sin_addr.s_addr=INADDR_ANY;

    int reuseaddr_opt_udp = 1;
    ret = setsockopt(sock_udp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp, sizeof(reuseaddr_opt_udp));

    ret = bind(sock_udp,(const sockaddr*)&my_addr,sizeof(my_addr));
    if (ret < 0)
      print_err("Error while binding the UDP Server.\n");

    pthread_t listerUDP;
    ret = pthread_create(&listerUDP, NULL, listener_update_thread_handler_UDP, NULL);
    if (ret != 0)
      print_err("Cannot create listener_update_thread_handler_UDP.\n");

    ret = pthread_detach(listerUDP);
    if (ret != 0)
      print_err("Cannot detach");

    pthread_t senderUDP;
    ret = pthread_create(&senderUDP, NULL, sender_update_thread_handler_UDP, NULL);
    if (ret != 0)
      print_err("Cannot create sender_update_thread_handler_UDP.\n");

    ret = pthread_detach(senderUDP);
    if (ret != 0)
      print_err("Cannot detach");

    /*
    pthread_t prova;
    ret = pthread_create(&prova, NULL, prova_func, NULL);
    if (ret != 0)
      print_err("Cannot create prova.\n");

    ret = pthread_detach(prova);
    if (ret != 0)
      print_err("Cannot detach");
    */

    usleep(1000);
    printf("\n***** Premere CTRL+C per chiudere il server. *****\n\n");

    /*
    starting server TCP -> IT WORKS !
                          */
  int socket_desc, client_desc;

  // some fields are required to be filled with 0
  struct sockaddr_in server_addr = {0};

  int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

  // initialize socket for listening
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  if (socket_desc < 1)
      print_err("Could not create socket");

  server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(SERVER_PORT_TCP); // don't forget about network byte order!

  /* We enable SO_REUSEADDR to quickly restart our server after a crash:
   * for more details, read about the TIME_WAIT state in the TCP protocol */
  int reuseaddr_opt = 1;
  ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  if (ret < 0)
      print_err("Cannot set SO_REUSEADDR option");

  // bind address to socket
  ret = bind(socket_desc, (struct sockaddr*) &server_addr, sockaddr_len);
  if (ret < 0)
      print_err("Cannot bind address to socket");

  // start listening
  ret = listen(socket_desc, 10);
  if (ret < 0)
      print_err("Cannot listen on socket");

  // we allocate client_addr dynamically and initialize it to zero
  struct sockaddr_in* client_addr = (sockaddr_in*)calloc(1, sizeof(sockaddr_in));

  // loop to handle incoming connections serially
  while (1) {
      client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
      if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
      if (client_desc < 0)
          print_err("Cannot open socket for incoming connection");

      if (DEBUG)
        printf("New Client connected!\n");

      /* We pass the socket descriptor and the address information
       * for the incoming connection to the handler. */
      //connection_handler(client_desc, client_addr);

      pthread_t p;
      Thread_arg_TCP st;
      st.surface_texture=surface_texture;
      st.elevation_texture=surface_elevation;
      st.socket=client_desc;

      ret=pthread_create(&p,NULL,thread_handler_TCP,(void*)&st);
      if(ret!=0)  print_err("Cannot create the thread");

      ret=pthread_detach(p);
      if(ret!=0)  print_err("Cannot detach");

      // reset fields in client_addr
      memset(client_addr, 0, sizeof(struct sockaddr_in));
  }

  free(client_addr);

  return 0;
}

/*
1) server UDP su un thread in preparazione dei client che si connettono, rimane sempre in attesa delle connessioni
  (rimane in attesa di aggiornamenti dei giocatori)
2) server TCP(sequenziale) per inizializzare i giocatori, setting della connessione con i varii dati del giocatore

*/
