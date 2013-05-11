#include "lsp.h"
# include <signal.h>

struct lsp_worker_list
{
  
    char *payload;
    int worker_id;
    int source_id;
    bool available;
    struct lsp_worker_list* next;

};


int timeout;
pthread_mutex_t timeout_lock;


struct request_queue
{
    char* crack_request;
    int conn_id;
    struct request_queue* next;
};

struct result_queue
{
    char* result;
    int conn_id;
    struct result_queue* next;
};


struct lsp_worker_list* worker_head;
struct lsp_worker_list* dead_head;
struct request_queue* request_head;
struct result_queue* result_head;

char* global_payload;
int global_payload_length;
uint32_t assign_worker();

void check_alive(lsp_server*);

void init_server_level_queues()
{
    
    // Initialize Worker Queue 
    struct lsp_worker_list *worker_new = (struct lsp_worker_list *) malloc(sizeof(struct lsp_worker_list));
    worker_new->worker_id=0;
    worker_new->next=NULL;
    worker_new->available=true;
    worker_head=worker_new;

    // Intialize Request Queue
    struct request_queue *request_new = (struct request_queue *) malloc(sizeof(struct request_queue));
    request_new->conn_id=0;
    request_new->next=NULL;
    request_new->crack_request="";
    request_head=request_new;

    // Intialize Result Queue
    struct result_queue *result_new = (struct result_queue *) malloc(sizeof(struct result_queue));
    result_new->conn_id=0;
    result_new->next=NULL;
    result_new->result="";
    result_head=result_new;

struct lsp_worker_list *dead_workers = (struct lsp_worker_list *) malloc(sizeof(struct lsp_worker_list));
    dead_workers->worker_id=0;
    dead_workers->next=NULL;
    dead_workers->available=true;
    dead_head=dead_workers;
}

bool  handle_payload(char* packet,uint32_t source_conid,int lth)
{
    uint32_t assigned_id=0;
    char* ch=packet;
  
 
    if( ch[0] == 'j')
        {
            // Add this to Worker Queue
                    
            struct lsp_worker_list *worker_add = (struct lsp_worker_list *) malloc(sizeof(struct lsp_worker_list));
            worker_add->worker_id=source_conid;
            worker_add->source_id=0;
            worker_add->next=NULL;
            worker_add->available = true;

            struct lsp_worker_list *worker_iterator;
            worker_iterator = worker_head;
            if(worker_iterator->next !=NULL)
                {
                    while(worker_iterator->next !=NULL)
                        {
                            worker_iterator = worker_iterator->next;
                        }
                }
            worker_iterator->next=worker_add;
            printf("\nWorker added\n");
            return true;

        }
 
    else if(ch[0]=='c')
        {
             
            // Add the request to the request_queue first
       
            struct request_queue *request_node = (struct request_queue *) malloc(sizeof(struct request_queue));
            request_node->conn_id=source_conid;
            request_node->crack_request = malloc(lth+1);      
       
       
            memset(request_node->crack_request,0,lth+1);
            memcpy(request_node->crack_request,(void*)packet,lth);       
            request_node->next = NULL;

      

            struct request_queue *request_iterator;
            request_iterator = request_head;
       
            if(request_iterator->next != NULL)
                {
                    while(request_iterator->next !=NULL)
                        {
                            request_iterator=request_iterator->next;
                        }
                } 
   
            request_iterator->next = request_node;

            printf("\nRequest from client number %d Queued \n",source_conid);
           
      
            return true;
     
        }
    else if (ch[0]=='f' || ch[0]=='x')
        {
               
            struct lsp_worker_list *worker_iter;
            worker_iter = worker_head;
            bool flg=true;
            if(worker_iter->next !=NULL)
                {
                    while(worker_iter->next !=NULL)
                        {
                            worker_iter = worker_iter->next;
                            if(worker_iter->worker_id == source_conid)
                                {
                                    worker_iter->available = true;
                                    flg=false;
                    
                                    // The result is found so it will be added to Result Queue

                                    struct result_queue *result_node = (struct result_queue *) malloc(sizeof(struct result_queue));
                                    result_node->conn_id=worker_iter->source_id;
                                    result_node->result = malloc(strlen(packet));
				    	

                                    memset(result_node->result,0,strlen(packet));
                                    memcpy(result_node->result,(void*)packet,strlen(packet));
                                    result_node->next = NULL;

                                    struct result_queue *result_iterator;
                                    result_iterator = result_head;

                                    if(result_iterator->next != NULL)
                                        {
                                            while(result_iterator->next !=NULL)
                                                {
                                                    result_iterator=result_iterator->next;
                                                }
                                        }

                                    result_iterator->next = result_node;

                   
              
                                }
                        }
                }

            return true;

        }
   
    else
        return true;
}

// This function iterates the Request Queue and Assigns a Worker if Available

uint32_t assign_worker()
{
    uint32_t id=0;

    // We should deque the top request
    struct request_queue *request_assign;
    struct request_queue *temporary;
    temporary = request_head;
    bool not_found=true;
    
    if(temporary->next !=NULL)
        {
            request_assign = temporary->next;

            struct lsp_worker_list *worker_iterator;
            worker_iterator = worker_head;
            bool fg=true;

            if(worker_iterator->next !=NULL)
                {
                    while(worker_iterator->next !=NULL && not_found)
                        {
                            worker_iterator = worker_iterator->next;
                            if(worker_iterator->available == true)
                                {

                                    worker_iterator->available = false;
                                    id = worker_iterator->worker_id;
                                    worker_iterator->source_id=request_assign->conn_id;
                                    fg=false;
                                    not_found = false;
              
                                    global_payload = malloc(strlen(request_assign->crack_request)+1);
				    worker_iterator->payload = global_payload;
	
                                    memset(global_payload,0,strlen(request_assign->crack_request));
                                    memcpy(global_payload,request_assign->crack_request,strlen(request_assign->crack_request));
                                    global_payload_length = strlen(request_assign->crack_request);
              
             

                                    temporary->next = request_assign->next;
                                    free(request_assign);
                                }
                        }
                }
        }

    return id;

}


void timeout_func(int sig)
{
	pthread_mutex_lock(&timeout_lock);
	timeout = 1;
	pthread_mutex_unlock(&timeout_lock);

}

int main(int argc, char** argv) 
{
    lsp_set_drop_rate(0.5);
    timeout = 0;
    pthread_mutex_init(&timeout_lock,NULL);
    signal(SIGALRM,timeout_func);
     alarm(2);
     if(argc == 2)
        {
            srand(12345);
            lsp_server* myserver = lsp_server_create(atoi(argv[1]));
	
            uint8_t payload[4096];
            uint32_t returned_id;
            int bytes_read;

            init_server_level_queues();

            struct result_queue *result_iterator_main;
            struct result_queue *temporary_main;
            char *payload_main;
    

        
	
            for(;;)
                {
              		
                    memset(payload,0,4096);
                    int bytes = lsp_server_read(myserver, payload, &returned_id);
        
        
                    if(bytes>0)
                        {
                
                            	//printf("Bytes read by server are %d\n",bytes);
				handle_payload((void*)payload,returned_id,bytes);
                
                        }
        
                    returned_id = assign_worker();
                    if(returned_id !=0)
                        {
                            printf("Worker Found: Request sent to worker : %d\n",returned_id);
                            lsp_server_write(myserver, (void *)global_payload,global_payload_length, returned_id);
                   //if(global_payload_length>0)
		    //printf("Bytes sent by the server are %d\n",global_payload_length);
                        }
		    //Check if worker is dead
		    if(timeout){
                    	check_alive(myserver);
		    }
                    // We will send the results to respective clients

                    result_iterator_main=result_head;
                    temporary_main=result_head;
        
                    if(result_iterator_main->next !=NULL)
                        {
                            while(result_iterator_main->next !=NULL)
                                {
                                    result_iterator_main = result_iterator_main->next;
                       
                                    payload_main= malloc(strlen(result_iterator_main->result));
                                    memset(payload_main,0,strlen(result_iterator_main->result));
                                    memcpy(payload_main,result_iterator_main->result,strlen(result_iterator_main->result));

                                    // Send to the respective client

                                    lsp_server_write(myserver, (void *)payload_main,strlen(result_iterator_main->result),result_iterator_main->conn_id);
                       
                                    temporary_main->next = result_iterator_main->next;
                                    free(result_iterator_main);
                                }
                        }

                

                }
	
            return 0;
        }
    else
        {
            printf("\nEnter correct number of arguments\n");
        }
}

void reassign_work(struct lsp_worker_list *deadw){
	struct request_queue *temporary;
        struct request_queue *reassignment;
	temporary = request_head;
        while(temporary->next!=NULL)
           {  
              temporary=temporary->next;
           }  	
        reassignment =(struct request_queue *)malloc(sizeof(struct request_queue)); 
	memset(reassignment,0,(sizeof(struct request_queue)));
	reassignment->conn_id =deadw->source_id;
	reassignment->crack_request = deadw->payload;
	temporary->next = reassignment;
	reassignment->next = NULL;

}

void check_alive(lsp_server * lead)
{
    
    struct lsp_worker_list *previous_worker = worker_head;
    struct lsp_worker_list *worker_it = worker_head;
    struct lsp_worker_list *dead_it= dead_head;
	pthread_mutex_lock(&timeout_lock);
	timeout = 0;
	pthread_mutex_unlock(&timeout_lock);
    signal(SIGALRM,timeout_func);
     alarm(2);
    while(worker_it->next!=NULL)
        {
        	previous_worker = worker_it;
		worker_it = worker_it->next;
            if(lsp_server_write(lead,"abcde",5,worker_it->worker_id))
                continue;
            else
                //add it to dead list
                {
                 printf("Worker with id %d is dead!!\n",worker_it->worker_id);  
		  while(dead_it->next!=NULL)
                        {
                            dead_it=dead_it->next;
                        }
                    struct lsp_worker_list *deadworker = worker_it;
                    previous_worker->next = worker_it->next;
		    if(worker_it->available==false)
	             reassign_work(worker_it);//Reassign the worker crack request to request queue
		    printf("Added to dead list the worker with id %d\n",worker_it->worker_id);
		     //free(worker_it);
		     dead_it->next= deadworker;
                    deadworker->next = NULL;
                }
	}
}
