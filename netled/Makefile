CC=gcc

ifeq ($(DEBUG),1)
GFLAGS=-g -D_DEBUG=1
else
GFLAGS=-fomit-frame-pointer -O2 -U_DEBUG
endif

_CFLAGS=$(CFLAGS) $(GFLAGS) -Wall -fPIC -pthread
LIBS=-lpcap
_LDFLAGS=$(LDFLAGS) $(LIBS) -L/usr/local/lib
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
	$(CC) $(_CFLAGS) -c -o $@ $<
	$(CC) -MM $(_CFLAGS) $< > $(patsubst %.c, %.d, $<)

$(ELFBIN): sniffer.o
	$(CC) $(_CFLAGS) $(_LDFLAGS) $+ -o $(ELFBIN)
	chmod +x $(ELFBIN)

install: $(ELFBIN)
	cp ./$(ELFBIN) $(DESTDIR)/sbin/$(ELFBIN)

