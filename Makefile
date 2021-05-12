CC ?= cc
CFLAGS += -D_POSIX_C_SOURCE=199309L -std=c99 -Wall
LDFLAGS +=

TARGET = diskgraph
SRC = diskgraph.c
OBJ = $(SRC:.c=.o)

all:	$(TARGET)

$(TARGET):	$(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o $(TARGET)
	@echo All clean
