TARGET=camera
SRC=main.c pint_glfw.c shader.c texture.c mesh.c
LIBS=-lglut -lGL -lglfw -lnetpbm
CFLAGS=-g -Wall -I/usr/include/netpbm

.PHONY: clean

OBJS := $(patsubst %.c,%.o,$(SRC))

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	gcc -o $@ $(OBJS) $(LIBS)

clean:
	rm -rf $(OBJS) $(TARGET)
