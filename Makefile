VERSION = "0_1_0"
CC = gcc
#CFLAGS = -DVERSION=\"$(VERSION)\" -Ofast -s
CFLAGS = -DVERSION=\"$(VERSION)\" -g
LDLIBS = -lm -lraylib

SOURCES = peanut_gb.c lcd.c meta.c raylib_backend.c main.c
OBJECTS = $(SOURCES:.c=.o)
OUTPUT = 3dgb

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) $(OUTPUT)