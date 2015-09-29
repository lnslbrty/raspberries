CC=gcc

ifeq ($(DEBUG),1)
GFLAGS=-g -D_NO_DEBUG=0 -D_GNU_SOURCE=1
else
GFLAGS=-fomit-frame-pointer -O2 -D_GNU_SOURCE=1
endif

WINCLUDES?=.
WLIBDIR?=.
CFLAGS=$(GFLAGS) -Wall -fPIC
LDFLAGS=-pthread -lwiringPi -lstrophe -L/usr/local/lib
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))
DEPS=$(patsubst %.c, %.d, $(SOURCES))
ELFBIN=iralarmd
CTLBIN=iralarmctl
SHBIN=iralarmshell
TESTBIN=test


all: $(OBJECTS) $(ELFBIN) $(CTLBIN) $(TESTBIN)

clean:
	-$(RM) $(OBJECTS)
	-$(RM) $(DEPS)
	-$(RM) $(ELFBIN) $(CTLBIN) $(SHBIN)
	-$(RM) $(TESTBIN)

%.o: %.c
	$(CC) -I$(WINCLUDES) $(CFLAGS) -c -o $@ $<
	$(CC) -MM $(CFLAGS) $< > $(patsubst %.c, %.d, $<)

$(TESTBIN): irthread.o irshmem.o
	$(CC) -L$(WLIBDIR) $(LDFLAGS) irthread.o irshmem.o test.o -o $(TESTBIN)
	chmod +x $(TESTBIN)

$(ELFBIN): irxmpp.o irpio.o irthread.o irshmem.o iralarmd.o
	$(CC) -L$(WLIBDIR) $(LDFLAGS) $+ -o $(ELFBIN)
	chmod +x $(ELFBIN)

$(CTLBIN): irshmem.o iralarmctl.o
	$(CC) -L$(WLIBDIR) $(LDFLAGS) $+ -o $(CTLBIN)
	[ -x $(SHBIN) ] || ln -s $(CTLBIN) $(SHBIN)
	chmod +x $(CTLBIN)

rebuild: clean all

install: $(ELFBIN) $(CTLBIN)
	cp ./$(ELFBIN) $(DESTDIR)/sbin/$(ELFBIN)
	cp ./$(CTLBIN) $(DESTDIR)/bin/$(CTLBIN)

