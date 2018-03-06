CCOPTS= -Wall -Wstrict-prototypes -g -std=gnu99 # this doesn't work: -Wno-deprecated-declarations
LIBS= -lm -lpthread -framework GLUT -framework OpenGL # change with -lglut -lGLU -lGL if run in Linux
CC=gcc
AR=ar

BINS=libso_game.a\
     so_game\
     test_packets_serialization 

OBJS = vec3.o\
       linked_list.o\
       surface.o\
       image.o\
       vehicle.o\
       world.o\
       world_viewer.o\
       so_game_protocol.o\

HEADERS=helpers.h\
	image.h\
	linked_list.h\
	so_game_protocol.h\
	surface.h\
	vec3.h\
	vehicle.h\
	world.h\
	world_viewer.h\

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all

all:	$(BINS)

libso_game.a: $(OBJS) 
	$(AR) -rcs $@ $^
	$(RM) $(OBJS)

so_game: so_game.c libso_game.a 
	$(CC) $(CCOPTS) -Ofast -o $@ $^ $(LIBS) 

test_packets_serialization: test_packets_serialization.c libso_game.a  
	$(CC) $(CCOPTS) -Ofast -o $@ $^  $(LIBS)

clean:
	rm -rf *.dSYM *.o *~  $(BINS)
