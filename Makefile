CCOPTS= -Wall -Wstrict-prototypes -g -std=gnu99 # this doesn't work: -Wno-deprecated-declarations
LIBS= -lm -lpthread -framework GLUT -framework OpenGL 
CC=gcc
AR=ar

BINS=libso_game.a\
     so_game\
   	 so_game_server\
	 so_game_client\
     test_packets_serialization\
     fake_so_game_server

OBJS = vec3.o\
       linked_list.o\
       surface.o\
       image.o\
       vehicle.o\
       world.o\
       world_viewer.o\
       so_game_protocol.o\
       stream_socket.o\
       fake_so_game_server.o

HEADERS=helpers.h\
	image.h\
	linked_list.h\
	so_game_protocol.h\
	surface.h\
	vec3.h\
	vehicle.h\
	world.h\
	world_viewer.h\
	stream_socket.h

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c $< -o $@

.phony: clean all

all:	$(BINS)

libso_game.a: $(OBJS) 
	$(AR) -rcs $@ $^
	$(RM) $(OBJS)

so_game: so_game.c libso_game.a 
	$(CC) $(CCOPTS) -Ofast -o $@ $^ $(LIBS) 

so_game_client: so_game_client.c libso_game.a
	$(CC) $(CCOPTS) -Ofast $^ -o $@ $(LIBS)  

so_game_server: so_game_server.c libso_game.a 
	$(CC) $(CCOPTS) -Ofast $^ -o $@  $(LIBS) 

fake_so_game_server: fake_so_game_server.c libso_game.a
	$(CC) $(CCOPTS) -Ofast $^ -o $@  $(LIBS) 

test_packets_serialization: test_packets_serialization.c libso_game.a  
	$(CC) $(CCOPTS) -Ofast -o $@ $^  $(LIBS)
	
clean:
	rm -rf *.dSYM *.o *~ $(BINS)
