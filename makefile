SHELL = /bin/sh

CC = gcc

CFLAGS = 
CPPFLAGS = -DLINUX -DMEDIACENTER
LDFLAGS =
LIBS =
ODIR = x86
ODIR64 = x64
ODIRARM = arm

_OBJS = fileio.o linuxserio.o lowlevel.o server.o errormessage.o flashrom.o webserver.o xap.o lanio.o ascii.o mce.o xbmc.o
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))
OBJS64 = $(patsubst %,$(ODIR64)/%,$(_OBJS))
OBJSARM = $(patsubst %,$(ODIRARM)/%,$(_OBJS))


irserver: $(OBJS) $(ODIR)/ccf.o 
	$(CC) $(CFLAGS) $(OBJS) $(ODIR)/ccf.o -m32 -o irserver $(LDFLAGS)

irserver_noccf: $(OBJS) $(ODIR)/noccf.o
	$(CC) $(CFLAGS) $(OBJS) $(ODIR)/noccf.o -m32 -o irserver $(LDFLAGS)

irserver64: $(OBJS64) $(ODIR64)/ccf.o 
	$(CC) $(CFLAGS) -DX64 $(OBJS64) $(ODIR64)/ccf.o -m64 -o irserver64 $(LDFLAGS)

irserver64_noccf: $(OBJS64) $(ODIR64)/noccf.o
	$(CC) $(CFLAGS) -DX64 $(OBJS64) $(ODIR64)/noccf.o -m64 -o irserver64 $(LDFLAGS)

irserver_arm: $(OBJSARM) $(ODIRARM)/ccf.o 
	$(CC) $(CFLAGS) $(OBJSARM) $(ODIRARM)/ccf.o -o irserver $(LDFLAGS)

irserver_arm_noccf: $(OBJSARM) $(ODIRARM)/noccf.o 
	$(CC) $(CFLAGS) $(OBJSARM) $(ODIRARM)/noccf.o -o irserver $(LDFLAGS)

all: irserver irserver64

arm: irserver_arm

arm_noccf: irserver_arm_noccf

clean:
	-rm $(OBJS) x86/noccf.o
	-rm $(OBJS64) x64/noccf.o
	-rm $(OBJSARM) arm/noccf.o


$(ODIR)/%.o: %.c dbstruct.h fileio.h lowlevel.h network.h serio.h pictures.h remote.h makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -m32 -c $< -o $@


$(ODIR64)/%.o: %.c dbstruct.h fileio.h lowlevel.h network.h serio.h pictures.h remote.h makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -DX64 -m64 -c $< -o $@

$(ODIRARM)/%.o: %.c dbstruct.h fileio.h lowlevel.h network.h serio.h pictures.h remote.h makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

irclient: client.c
	$(CC) $(CPPFLAGS) $(CFLAGS) client.c -o irclient $(LDFLAGS)

