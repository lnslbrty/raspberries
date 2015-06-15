CC=gcc

ifeq ($(DEBUG),1)
GFLAGS=-g -D_NO_DEBUG=0
else
GFLAGS=-fomit-frame-pointer -O2
endif

WINCLUDES?=.
WLIBDIR?=.
CFLAGS=$(GFLAGS) -Wall -fPIC
LDFLAGS=-pthread -lwiringPi -lpcap -L/usr/local/lib
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))
DEPS=$(patsubst %.c, %.d, $(SOURCES))
ELFBIN=netled


all: $(OBJECTS) $(ELFBIN)

clean:
	-$(RM) $(OBJECTS)
	-$(RM) $(DEPS)
	-$(RM) $(ELFBIN)

%.o: %.c
	$(CC) -I$(WINCLUDES) $(CFLAGS) -c -o $@ $<
	$(CC) -MM $(CFLAGS) $< > $(patsubst %.c, %.d, $<)

$(ELFBIN): sniffer.o
	$(CC) -L$(WLIBDIR) $(LDFLAGS) $+ -o $(ELFBIN)
	chmod +x $(ELFBIN)

install: $(ELFBIN)
	cp ./$(ELFBIN) $(DESTDIR)/sbin/$(ELFBIN)

