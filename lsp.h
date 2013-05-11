#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include "lspmessage.pb-c.h"

// Global Parameters. For both server and clients.

#define _EPOCH_LTH 2.0
#define _EPOCH_CNT 5;
#define _DROP_RATE 0.0;

void lsp_set_epoch_lth(double lth);
void lsp_set_epoch_cnt(int cnt);
void lsp_set_drop_rate(double rate);

typedef struct
{
    int nextseq;
    int sd; // Socket
    struct sockaddr_in serveraddr;
    struct client_queue_to_server *outputlist;
    struct client_queue_from_server *inputlist;
    int connectionID;
    int last_packet_received;
    int retry_count;
} lsp_client;

typedef struct lsp_client_list_t
{
  struct sockaddr_in  clientaddr;
  int ID;
  int expected_seq_num;
  int expected_seq_num_to_client;
  struct lsp_client_list_t *next;
  int clientfd;
    int retrycount;
}lsp_client_list;


// We are defining the structure of the Queues 

// Server side Queue

struct from_client
{
LSPMessage msg;
struct from_client *next; 
};

struct to_client
{
LSPMessage msg;
struct to_client *next;
};


// Queues at the Client Side

struct client_queue_from_server
{
LSPMessage msg;
struct client_queue_from_server *next;
};

struct client_queue_to_server
{
LSPMessage msg;
struct client_queue_to_server *next;
};

lsp_client* lsp_client_create(const char* dest, int port);
int lsp_client_read(lsp_client* a_client, uint8_t* pld);
bool lsp_client_write(lsp_client* a_client, uint8_t* pld, int lth);
bool lsp_client_close(lsp_client* a_client);



// Defining the structure for Woerker Class

/*typedef struct
{
    int sd; //Socket
    struct sockaddr_in serveraddr;
} lsp_worker;*/


typedef struct
{
   int sd;  // Socket 
   struct sockaddr_in ServAddr; // Server Address
} lsp_server;

lsp_server* lsp_server_create(int port);
int  lsp_server_read(lsp_server* a_srv, void* pld, uint32_t* conn_id);
bool lsp_server_write(lsp_server* a_srv, void* pld, int lth, uint32_t conn_id);
bool lsp_server_close(lsp_server* a_srv, uint32_t conn_id);
int init_server_queues();
int populate_server_input_queue();
int populate_client_list(struct sockaddr_in address,LSPMessage *m);
