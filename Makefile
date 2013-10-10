CC = gcc

OS = $(shell uname -s)
ifeq ($(OS), Darwin)
CFLAGS = -lusb-1.0 -framework CoreFoundation -framework IOKit -I./include
endif
ifeq ($(OS), Linux)
CFLAGS = -lusb-1.0
endif
ifndef CFLAGS
$(error Unsupported OS: Use Linux or OS X)
endif

all:
		$(CC) bdu.c -o bdu $(CFLAGS)
		
		arm-linux-gnu-as -mthumb --fatal-warnings -o payload.o payload.S
		arm-linux-gnu-objcopy -O binary  payload.o payload.bin
		rm payload.o

clean:
		-rm bdu
		-rm payload.bin

rebuild: clean all

