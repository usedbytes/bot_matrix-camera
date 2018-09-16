TARGET=camera
SRC=main.c shader.c texture.c mesh.c drawcall.c list.c compositor.c campipe.c font.c
LDFLAGS=-lnetpbm -lm -lpng
CFLAGS=-g -Wall -I/usr/include/netpbm

ifeq ($(PINT),glfw)
    SRC += pint_glfw.c feed_nocamera.c readpixels_shared_fbo.c
    LDFLAGS +=-lGL -lglfw -lglut
    CFLAGS += -DFRAGMENT_SHADER=\"fragment_shader.glsl\"
else ifeq ($(PINT),piegl)
    SRC += pint_piegl.c camera.c cameracontrol.c feed_camera.c matrix.c vcsm_shared_fbo.c
    CFLAGS += -DFRAGMENT_SHADER=\"fragment_external_oes_shader.glsl\"
    CFLAGS += -DUSE_SHARED_FBO=1
    CFLAGS +=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi
    CFLAGS +=-I$(SDKSTAGE)/opt/vc/include/ -I$(SDKSTAGE)/opt/vc/include/interface/vcos/pthreads -I$(SDKSTAGE)/opt/vc/include/interface/vmcs_host/linux -I./
    LDFLAGS +=-L$(SDKSTAGE)/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lmmal_core -lmmal_util -lmmal_vc_client -lvcsm -lpthread
endif

.PHONY: clean

OBJS := $(patsubst %.c,%.o,$(SRC))

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	gcc -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -rf $(OBJS) $(TARGET)
