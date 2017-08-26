TARGET=camera
SRC=main.c pint_glfw.c
LIBS=-lglut -lGL -lglfw
CFLAGS=-g -Wall

OBJS := $(patsubst %.c,%.o,$(SRC))

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	gcc -o $@ $(OBJS) $(LIBS)
