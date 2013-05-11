CC = gcc -g

TARGET = request server worker

CFLAGS += -I/opt/local/include
LDFLAGS += -lprotobuf-c -lpthread -lcrypto -L/opt/local/lib

all:	$(TARGET)

$(TARGET): lsp.o lspmessage.pb-c.o
	$(CC) -o $@ $@.c $^ $(LDFLAGS)

%.o:	%.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o 
	rm -f $(TARGET)

