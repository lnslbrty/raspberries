CC=gcc

ifeq ($(DEBUG),1)
GFLAGS=-g -D_NO_DEBUG=0 -D_GNU_SOURCE=1
else
GFLAGS=-fomit-frame-pointer -O2 -D_GNU_SOURCE=1
endif

WINCLUDES?=.
WLIBDIR?=.
CFLAGS=$(GFLAGS) -Wall -fPIC
LDFLAGS=-pthread
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))
GDEPS=

ifneq ($(DO_XMPP),)
CFLAGS += -DDO_XMPP=1
LDFLAGS += -lstrophe
GDEPS += irxmpp.o
endif

ELFBIN=iralarmd
ELFBIN_DEPS=irpio.o irthread.o irshmem.o iralarmd.o
CTLBIN=iralarmctl
SHBIN=iralarmshell
TESTBIN=test

all: $(ELFBIN) $(CTLBIN) $(TESTBIN)

clean:
	-$(RM) $(OBJECTS)
	-$(RM) $(ELFBIN) $(CTLBIN) $(SHBIN)
	-$(RM) $(TESTBIN)

%.o: %.c
	$(CC) -I$(WINCLUDES) $(CFLAGS) -c -o $@ $<

$(TESTBIN): irthread.o irshmem.o $(GDEPS)
	$(CC) -L$(WLIBDIR) $(LDFLAGS) irthread.o irshmem.o test.c -o $(TESTBIN)
	chmod +x $(TESTBIN)

$(ELFBIN): irpio.o irthread.o irshmem.o iralarmd.o $(GDEPS)
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

