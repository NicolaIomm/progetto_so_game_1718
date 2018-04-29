
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

#define DEBUG 1
#define SERVER_PORT 3000
#define INCOMING_DATA_SIZE 1000000

int global_id=1;

typedef struct thread_arg
{
 Image* surface_texture;
 Image* elevation_texture;
 int socket;

} Thread_arg;

void* thread_handler_UDP(void* arg){
    printf("Prova\n");
}

void* thread_handler_TCP(void* arg){
  Thread_arg* tr=(Thread_arg*)arg;
  Image* srf_text=tr->surface_texture;
  Image* elv_text=tr->elevation_texture;
  int socket_desc=tr->socket;
  int ret;

  char data[INCOMING_DATA_SIZE];
    int data_len;

    data_len = receivePacket(socket_desc, data);
    IdPacket* received_id_packet = (IdPacket*) Packet_deserialize(data, data_len);

    printf("Received requestIDPacket\n");

    int id_received = received_id_packet->id;
    if (id_received == -1){
            // Build IdPacket to ask an ID from Server
        IdPacket* request_id_packet = (IdPacket*)malloc(sizeof(IdPacket));
        PacketHeader id_header;
        id_header.type = GetId;
        request_id_packet->header = id_header;
        request_id_packet->id = global_id;
        printf("Client connected with ID: %d\n",global_id);
        global_id++;
        // Sent deserialized IdPacket
        data_len = Packet_serialize(data, &request_id_packet->header);
        sendPacket(socket_desc, data, data_len);
        Packet_free((PacketHeader*) request_id_packet);
    }

    Packet_free((PacketHeader*) received_id_packet);

    printf("Sent a new IDPacket to client\n");

      // Reveive the player's texture from client
    data_len = receivePacket(socket_desc, data);
    ImagePacket* received_profile_texture = (ImagePacket*) Packet_deserialize(data, data_len);

    Image* profile_texture_image = received_profile_texture->image;

    Packet_free((PacketHeader*) received_profile_texture);

    printf("Received profile texture from client\n");

      // Resend the player's texture to client to confirm the texture
    ImagePacket* confirmed_player_texture = (ImagePacket*)malloc(sizeof(ImagePacket));
    PacketHeader confirmed_player_texture_header;
    confirmed_player_texture_header.type = PostTexture;
    confirmed_player_texture->header = confirmed_player_texture_header;
    confirmed_player_texture->id = global_id;
    confirmed_player_texture->image = profile_texture_image;

    data_len = Packet_serialize(data, &confirmed_player_texture->header);
    sendPacket(socket_desc, data, data_len);

    Packet_free((PacketHeader*) confirmed_player_texture);

    /* End of your code */

    // close socket
    ret = close(socket_desc);
    if (ret < 0)
        print_err("Cannot close socket for incoming connection");

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

  /*printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
  Image* vehicle_texture = Image_load(vehicle_texture_filename);
  if (vehicle_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }*/

  // not needed here
  //   // construct the world
  // World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  // // create a vehicle
  // vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  // Vehicle_init(vehicle, &world, 0, vehicle_texture);

  // // add it to the world
  // World_addVehicle(&world, vehicle);



  // // initialize GL
  // glutInit(&argc, argv);
  // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  // glutCreateWindow("main");

  // // set the callbacks
  // glutDisplayFunc(display);
  // glutIdleFunc(idle);
  // glutSpecialFunc(specialInput);
  // glutKeyboardFunc(keyPressed);
  // glutReshapeFunc(reshape);

  // WorldViewer_init(&viewer, &world, vehicle);


  // // run the main GL loop
  // glutMainLoop();

  // // check out the images not needed anymore
  // Image_free(vehicle_texture);
  // Image_free(surface_texture);
  // Image_free(surface_elevation);

  // // cleanup
  // World_destroy(&world);

  /*
    starting server UDP
  */

    int ret;
    Thread_arg prova;


  //prova UDP

    int ds_sock;
    struct sockaddr_in my_addr;
    struct sockaddr addr;
    int addrlen;
    char buff[1024];

    ds_sock=socket(AF_INET, SOCK_DGRAM,0);

    my_addr.sin_family=AF_INET;
    my_addr.sin_port=3001;
    my_addr.sin_addr.s_addr=INADDR_ANY;

    bind(ds_sock,(const sockaddr*)&my_addr,sizeof(my_addr));
    if(recvfrom(ds_sock,buff,1024,0,(struct sockaddr*)&addr,(socklen_t*)&addrlen)==-1){
        printf("Error in recvfrom\n");
        exit(1);
    }

    printf("OK\n");
    pthread_t pp;
    ret=pthread_create(&pp,NULL,thread_handler_UDP,(void*)&prova);
    printf("Thread created (UDP)\n");

    ret=pthread_detach(pp);
    if(ret!=0)  print_err("Cannot detach");
  /*
    starting server TCP
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
  server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

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
  ret = listen(socket_desc, 1);
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

      if (DEBUG) fprintf(stderr, "Incoming connection accepted...\n");

      /* We pass the socket descriptor and the address information
       * for the incoming connection to the handler. */
      //connection_handler(client_desc, client_addr);

      pthread_t p;
      Thread_arg st;
      st.surface_texture=surface_texture;
      st.elevation_texture=surface_elevation;
      st.socket=client_desc;

      ret=pthread_create(&p,NULL,thread_handler_TCP,(void*)&st);
      if(ret!=0)  print_err("Cannot create the thread");

      printf("Thread created  (TCP)\n");

      ret=pthread_detach(p);
      if(ret!=0)  print_err("Cannot detach");






      // reset fields in client_addr
      memset(client_addr, 0, sizeof(struct sockaddr_in));
  }

  free(client_addr);

  exit(EXIT_SUCCESS); // this will never be executed


  return 0;
}

/*
1) server UDP su un thread in preparazione dei client che si connettono, rimane sempre in attesa delle connessioni
  (rimane in attesa di aggiornamenti dei giocatori)
2) server TCP(sequenziale) per inizializzare i giocatori, setting della connessione con i varii dati del giocatore

*/
