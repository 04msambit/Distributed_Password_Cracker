#include "lsp.h"
#include <pthread.h>
#include<time.h>
#include "lspmessage.pb-c.h"

#define BUFFER_LENGTH 1500

double epoch_lth = _EPOCH_LTH;
int epoch_cnt = _EPOCH_CNT;
double drop_rate = _DROP_RATE;
char buffer[BUFFER_LENGTH];
struct sockaddr_in client;
int globalID=1;

pthread_mutex_t clientoutput;
pthread_mutex_t clientinput;
pthread_mutex_t serveroutput;
pthread_mutex_t serverinput;
pthread_mutex_t clientlist;


pthread_t server_epoch_thread;
pthread_t client_epoch_thread;
// The below pointers are being declared to create the Head 

lsp_client_list *client_head;
struct from_client *from_client_head;
struct to_client   *to_client_head;
struct lsp_worker_list *worker_head;
//struct client_queue_from_server *client_queue_from_server_head;
//struct client_queue_to_server   *client_queue_to_server_head;
/*
 *
 *
 *				LSP RELATED FUNCTIONS
 *
 *
 */  

void lsp_set_epoch_lth(double lth){epoch_lth = lth;}
void lsp_set_epoch_cnt(int cnt){epoch_cnt = cnt;}
void lsp_set_drop_rate(double rate){drop_rate = rate;}


/*
 *
 *
 *				CLIENT RELATED FUNCTIONS
 *
 *
 */  

void *clientEpoch(void *client)
{

    int breakflag = 0;
    char buffer[BUFFER_LENGTH];
    uint8_t* buf,buf1,buf2,buf3;
    int i,rc,len,ack,len1,len2,len3;
    lsp_client *a_client;
    a_client = (lsp_client*)client;
    if(a_client==NULL)
        printf("NULL");
    int sockfd=0;
    struct sockaddr_storage addr;
    socklen_t fromlen;
    fd_set read_fd_set,master_set;
    fromlen = sizeof addr;
    sockfd = a_client->sd;
    FD_ZERO (&read_fd_set);
    FD_ZERO(&master_set);
    FD_SET (sockfd, &read_fd_set);
    FD_SET(sockfd,&master_set);
    a_client->retry_count = 0;
    
    while(1)
        {
            
            
            read_fd_set = master_set;
            struct timeval tv = {2, 0};
            i= select(FD_SETSIZE, &read_fd_set, NULL, NULL, &tv);
            //generate a random number 
            double prob = (double)rand()/(double)RAND_MAX;
            if (i==0)
                {
                    
                    //printf("The retry count is %d",a_client->retry_count);
                    a_client->retry_count++;//increase the retry count by 1 
                    if(a_client->retry_count>5){
                        //printf("The retry count is %d\n",a_client->retry_count);
                        break;
                    }
                    //send an acknoledgement for the most recently received data message or an acknowledgement with sequence number 0 if no data message have been received
                    
                    LSPMessage resendingack = LSPMESSAGE__INIT;
                    resendingack.connid = a_client->connectionID;
                    resendingack.seqnum = a_client->last_packet_received;//
                    resendingack.payload.data = NULL;
                    resendingack.payload.len = 0;
                    len=lspmessage__get_packed_size(&resendingack);
                    memset(&buf,0,sizeof(buf));
                    buf=malloc(len);
                    lspmessage__pack(&resendingack,buf);
                    rc = sendto(a_client->sd, buf, len, 0,  (struct sockaddr *)&a_client->serveraddr,  sizeof(a_client->serveraddr));
                    //printf("\nAck bytes resent by client for seq %d are %d",a_client->last_packet_received,rc);
                    
                    
                    
                    //data sent but not acknowledged then resend
                    pthread_mutex_lock(&clientoutput);//lock!!
                    if(a_client->outputlist!=NULL)
                        {
                            len = lspmessage__get_packed_size(&a_client->outputlist->msg);
                            memset(&buf,0,sizeof(buf));
                            buf= malloc(len);
                            lspmessage__pack(&a_client->outputlist->msg,buf);
                            rc = sendto(a_client->sd, buf, len, 0,  (struct sockaddr *)&a_client->serveraddr,  sizeof(a_client->serveraddr));
                            
                        }
                    pthread_mutex_unlock(&clientoutput);//unlock!!
                }
            else if (i>0)
                {
 
                    a_client->retry_count = 0;
                    //printf("We reset the retry count to %d",a_client->retry_count);
                    //receive message
                    ack = recvfrom(a_client->sd,buffer,sizeof(buffer),0,(struct sockaddr *)&addr,&fromlen);
                    
                    LSPMessage *receivedmessage;
                    receivedmessage = lspmessage__unpack(NULL,ack,buffer);
                    // check your own packets
                    if(receivedmessage->connid ==a_client->connectionID)
                        {
                            if(receivedmessage->payload.len==0)//its an ack then delete the corresponding message from the output queue
                                {
                                    pthread_mutex_lock(&clientoutput);//lock!!
                                   
                              
                                   
                                    if(a_client->outputlist==NULL)
                                        {
                                            //printf("No message in the outputlist!!!!");
                                        }
                                    else
                                        {
                                            if(a_client->outputlist->msg.seqnum==receivedmessage->seqnum)
                                                {
                                                    struct client_queue_to_server *iter = a_client->outputlist;
                                                    a_client->outputlist = a_client->outputlist->next;
                                                    free(iter);
                                                }
                                        }
                                    pthread_mutex_unlock(&clientoutput);//unlock!!
                                }

                            else
                                {
				  
                                        
                                    //if message is data then add it to input queue and send an ack(We check the sequence number is one greater than the last)
                                        
                                   

                                        if(receivedmessage->seqnum==a_client->last_packet_received+1)
                                        {
						//printf("Received msg at client with seq %d: %s\n",receivedmessage->seqnum,receivedmessage->payload.data);
                                            
                                            a_client->last_packet_received++; //We increase the count by 1 
                                            struct client_queue_from_server *node = (struct client_queue_from_server *)malloc(sizeof(struct client_queue_from_server));
                                            LSPMessage localmsg = LSPMESSAGE__INIT;
                                            node->msg = localmsg;
                                            node->msg.connid = receivedmessage->connid;
                                            node->msg.seqnum = receivedmessage->seqnum;
                                            node->msg.payload.data = malloc(receivedmessage->payload.len);
                                            node->msg.payload.len = receivedmessage->payload.len;
                                            memcpy(node->msg.payload.data,receivedmessage->payload.data,node->msg.payload.len);
                                            node->next = NULL;
                                            pthread_mutex_lock(&clientinput);//lockkk!!!
                                            struct client_queue_from_server *iterator = a_client->inputlist;
                                            if(iterator==NULL)
                                                {
                                                    a_client->inputlist=node;
                                                }
                                            else
                                                {
                                                    while(iterator->next !=NULL)
                                                        {
                                                            iterator = iterator->next;
                                                        }
                                                    iterator->next = node;
                                                }
                                         pthread_mutex_unlock(&clientinput); //unlock!!
                                            
                                        
                                                   
                                        }
                                        
                                        

                                  LSPMessage ackg = LSPMESSAGE__INIT;
                                  ackg.connid = a_client->connectionID;
                                  
                                  ackg.seqnum = receivedmessage->seqnum;
                                  ackg.payload.data = NULL;
                                  ackg.payload.len = 0;
                                  len=lspmessage__get_packed_size(&ackg);
                                  memset(&buf,0,sizeof(buf));
                                  buf=malloc(len);
                                  lspmessage__pack(&ackg,buf);
                                  //printf("We are sending ACK for %d\n",ackg.seqnum);
                                  rc = sendto(a_client->sd, buf, len, 0,  (struct sockaddr *)&a_client->serveraddr,  sizeof(a_client->serveraddr));

    
                           
                                }


                                                                  
                            
                        }
           
   	                 	LSPMessage ackg = LSPMESSAGE__INIT;
                                  ackg.connid = a_client->connectionID;

                                  ackg.seqnum = a_client->last_packet_received;
                                  ackg.payload.data = NULL;
                                  ackg.payload.len = 0;
                                  len=lspmessage__get_packed_size(&ackg);
                                  memset(&buf,0,sizeof(buf));
                                  buf=malloc(len);
                                  lspmessage__pack(&ackg,buf);
                                  //printf("We are sending ACK for %d\n",ackg.seqnum);
                                  rc = sendto(a_client->sd, buf, len, 0,  (struct sockaddr *)&a_client->serveraddr,  sizeof(a_client->serveraddr));

                                        


                    pthread_mutex_lock(&clientoutput);//lock!!
                    if(a_client->outputlist!=NULL)
                        {//To end after updating output list
                            
                                  
                            LSPMessage mesg = LSPMESSAGE__INIT;
                            mesg.connid = a_client->outputlist->msg.connid;
                            mesg.seqnum = a_client->outputlist->msg.seqnum;
                            mesg.payload.data = malloc(a_client->outputlist->msg.payload.len);
                            mesg.payload.len = a_client->outputlist->msg.payload.len;
                            memcpy(mesg.payload.data,a_client->outputlist->msg.payload.data,a_client->outputlist->msg.payload.len);
                         
                            len = lspmessage__get_packed_size(&mesg);
                            buf= malloc(len);
                            lspmessage__pack(&mesg,buf);
                            //printf("%f>%f:Sending data %s \n",prob,drop_rate,mesg.payload.data);
                            if(prob>drop_rate)//send packets randomly
                                rc = sendto(a_client->sd, buf, len, 0,  (struct sockaddr *)&a_client->serveraddr,  sizeof(a_client->serveraddr));
                            
                        }
                    pthread_mutex_unlock(&clientoutput); //unlock!!
                }   /* receive and send messages */
            else{
                perror("select error");
            }
        }
    
    return NULL;
}


//Establish a connection and create a new client

lsp_client* lsp_client_create(const char* src, int port)
{
   lsp_client *myclient=(lsp_client *)malloc(sizeof(lsp_client));
   int rc,ack,value,tc;
   int len;
   int count=0,success = 0;
   uint8_t* buf;   
   char message[]="";
   uint8_t* pld = (void *)message;
   int lth = strlen(message);
   int serveraddrlen = sizeof(struct sockaddr_in);
   fd_set read_fd_set,master_set;
   LSPMessage *marshelmessage;
   LSPMessage msg = LSPMESSAGE__INIT;
   
   
   myclient->sd = socket(AF_INET, SOCK_DGRAM, 0);
   
   FD_SET(myclient->sd,&master_set);
   memset(&myclient->serveraddr, 0, sizeof(myclient->serveraddr));
   myclient->serveraddr.sin_family      = AF_INET;
   myclient->serveraddr.sin_port        = htons(port);
   myclient->serveraddr.sin_addr.s_addr = inet_addr(src);
   struct sockaddr_storage addr;
   socklen_t fromlen;
   fromlen = sizeof addr;
   // Client will now send a request to establish a connection with the server. This will be done using the marshalling function 
   msg.connid = 0;
   msg.seqnum = 0;
   msg.payload.data = malloc(sizeof(uint8_t) * lth);
   msg.payload.len = sizeof(uint8_t)*lth;
   memcpy(msg.payload.data, pld, lth*sizeof(uint8_t));
   len=lspmessage__get_packed_size(&msg);
   buf=malloc(len);
   lspmessage__pack(&msg,buf);
   
   while(count<5){
       rc = sendto(myclient->sd, buf, len, 0,  (struct sockaddr *)&myclient->serveraddr,  sizeof(myclient->serveraddr));
       FD_ZERO (&read_fd_set);
       FD_SET (myclient->sd, &read_fd_set);
       struct timeval tv = {2, 0}; 
       value = select(myclient->sd +1, &read_fd_set, NULL, NULL, &tv);
       
       read_fd_set = master_set;
       if(value>0){
           ack = recvfrom(myclient->sd,buffer,sizeof(buffer),0,(struct sockaddr *)&addr,&fromlen);
           //Update server port in client
           myclient->serveraddr = *((struct sockaddr_in*)&addr);
           // printf("Updated port is %d\n",ntohs(myclient->serveraddr.sin_port));
           
           if (ack>0){
               success = 1;
               break;
           }
           else {
               count ++;
           }
       }
       else count++;
   }    
   if (success ==1){
   //printf("Client create %d\n",port); 
       marshelmessage=lspmessage__unpack(NULL,ack,buffer);
      
       free(buf);
       free(msg.payload.data);
       myclient->connectionID = marshelmessage->connid; //Assign the connection ID      
       myclient->last_packet_received = marshelmessage->seqnum;//seq num received
       myclient->nextseq = 1;//seqnum to be sent
       //       Spawn client epoch thread
       pthread_mutex_init(&clientoutput, NULL);
       pthread_mutex_init(&clientinput,NULL);
       tc = pthread_create(&client_epoch_thread,NULL,clientEpoch,(void*)myclient);
       assert(0==tc);
       count = 0;
       //Set drop rate in client
       // lsp_set_drop_rate(0.2);
       

       if(myclient==NULL)
            printf("client is null");
       return myclient;
   }
   else{ 
       count = 0;
       printf("Server not responding...\n");
       return NULL;
   }
}



int lsp_client_read(lsp_client* a_client, uint8_t* pld)
{
    int len=0; 
    //printf("LSP_CLIENT_READ :The retry count in lsp client read is %d",a_client->retry_count);
    if(a_client->retry_count>5)
        return -1;
    pthread_mutex_lock(&clientinput);//lock!! 
    struct client_queue_from_server *iterator = a_client->inputlist;
    if(iterator==NULL)
        {
        pthread_mutex_unlock(&clientinput);
        return 0;
        }
    else{
        memcpy(pld,iterator->msg.payload.data,iterator->msg.payload.len);
    }
 
    //printf("\n LSP CLIENT READ :Message from server %s with seqnum %d \n",iterator->msg.payload.data,iterator->msg.seqnum);

    len = iterator->msg.payload.len;
    //printf("\n Bytes read before unlock is %d",len);
    a_client->inputlist = iterator->next;
   	free(iterator);
    pthread_mutex_unlock(&clientinput);//unlock!!  
    return len;

}

bool lsp_client_write(lsp_client* a_client, uint8_t* pld, int lth)
{
    struct client_queue_to_server *node = (struct client_queue_to_server *)malloc(sizeof(struct client_queue_to_server));
    LSPMessage localmsg = LSPMESSAGE__INIT;
    node->msg = localmsg;
    node->msg.connid = a_client->connectionID;
    node->msg.seqnum = a_client->nextseq;
    a_client->nextseq +=1;
    node->msg.payload.data = malloc(sizeof(uint8_t)*lth);
    node->msg.payload.len = sizeof(uint8_t)*lth;
    memcpy(node->msg.payload.data,pld,lth*sizeof(uint8_t));
    node->next = NULL;

    pthread_mutex_lock(&clientoutput);//lock
    
    struct client_queue_to_server *iterator=a_client->outputlist; 
    if(iterator==NULL){
        a_client->outputlist=node;
        
    }
    else{
        while(iterator->next !=NULL){
            iterator = iterator->next;
        }
       
        iterator->next = node;
    }
   
    pthread_mutex_unlock(&clientoutput);
    //printf("LSP_CLIENT_WRITE:The retry count in lsp_client_write is %d",a_client->retry_count);
    if(a_client->retry_count>5)
        return false;
    return true;
}


bool lsp_client_close(lsp_client* a_client)
{
    pthread_kill(client_epoch_thread,0);
    close(a_client->sd);
    if(a_client->outputlist!=NULL)
        free(a_client->outputlist);
    if(a_client->inputlist!=NULL)
        free(a_client->inputlist);
    free(a_client);
}

/*
 *
 *
 *				SERVER RELATED FUNCTIONS
 *
 *
 */  

void *serverEpoch(void *server)
{
    uint8_t* buf;
    int i,rc,len;
    struct sockaddr_storage from;
    socklen_t    fromlen=sizeof(from);
    lsp_server *a_server;
    lsp_client_list* iterator = client_head;
    a_server = (lsp_server*)server;
    int fd;
    int workerfd;
    fd = a_server->sd;
    fd_set read_set,master_fd_set;
    FD_ZERO (&read_set);
    FD_ZERO(&master_fd_set);
    FD_SET (fd, &read_set);
    FD_SET(fd,&master_fd_set);
    
   

    while(1)
    {
        struct timeval ts = {2, 0};
         
         read_set = master_fd_set;
         i= select(FD_SETSIZE, &read_set, NULL, NULL, &ts);
         //generate a random number
         double prob = (double)rand()/(double)RAND_MAX;
         if (i==0)
          {
            

            // TASK 0 : ACK the connection request if no data message have been received

              pthread_mutex_lock(&clientlist);
              
              struct sockaddr_in destination_client_last;
              lsp_client_list *list_iterator = client_head;
              
              if(list_iterator->next !=NULL)
                  {
                      while(list_iterator->next !=NULL)
                          {
                              list_iterator = list_iterator->next;
                              if( list_iterator->expected_seq_num == 1)
                                  {
                                      //break for retry count greater than 5
                                       if(list_iterator->retrycount>5)
                                        continue;
                                      destination_client_last= list_iterator->clientaddr;
                                      // We will send the ACK
                                      LSPMessage destination_messag = LSPMESSAGE__INIT;
                                      destination_messag.connid = list_iterator->ID;
                                      destination_messag.seqnum = 0;  // Since it is ACK
                                      
                                      destination_messag.payload.data = NULL;
                                      destination_messag.payload.len = 0;
                                      
                                      memset(&buf,0,sizeof(buf));
                                      len=lspmessage__get_packed_size(&destination_messag);
                                      buf=malloc(len);
                                      lspmessage__pack(&destination_messag,buf);
                                      
                                      //printf("\n We are ACking the connection for connid %d\n",list_iterator->ID);
                                      
                                      
                                      rc = sendto(list_iterator->clientfd, buf, len, 0,  (struct sockaddr *)&destination_client_last,sizeof(destination_client_last));
                                      //Increase the retry count by 1
                                      list_iterator->retrycount ++;
                                       //if(list_iterator->retrycount>5) 
                                           //printf("The retry count for connection %d is %d\n",list_iterator->ID,list_iterator->retrycount); 
                                  }
                          } 
                  }

              pthread_mutex_unlock(&clientlist);
            

              // Task 1 - Data Message has been sent but not ACK , then resend it. This is for each connection 

           
              pthread_mutex_lock(&serveroutput);
              pthread_mutex_lock(&clientlist);
           
              lsp_client_list *client_iter = client_head;
              int exp_seq_num;
              int conid;
              int seq=1;
              struct sockaddr_in destination_last_client;
              struct to_client *output_iterator;
              if(client_iter->next != NULL) // This shows that there are clients in the Queue
                  {
                      while (client_iter->next!=NULL)
                          {
                              client_iter = client_iter->next;
                                       if(client_iter->retrycount>5)
                                        continue;
    
                              destination_last_client = client_iter->clientaddr;                    
       
                              exp_seq_num = client_iter->expected_seq_num_to_client;
                              conid = client_iter->ID;
                              void *payld;
                              int data_length;
                    

                              // We will iterate the input queue to get most recently received message
                    
                              output_iterator = to_client_head;
		
                              if(output_iterator->next !=NULL)

                                  {

                                      while(output_iterator->next !=NULL)
                                          {
                                              output_iterator = output_iterator->next;
                                              if(output_iterator->msg.connid == conid)
                                                  {
                                                      if(output_iterator->msg.seqnum < exp_seq_num)
                                                          {
                                                              seq= output_iterator->msg.seqnum;
                                                              data_length = output_iterator->msg.payload.len;    
                                                              payld= malloc(output_iterator->msg.payload.len);
                                                              memset(payld,0,output_iterator->msg.payload.len);
                                                              memcpy(payld,output_iterator->msg.payload.data,output_iterator->msg.payload.len); 
                                                              break;
                                                          }
                                                  }
                                          }



                                      // Now send the message

                                      LSPMessage mg = LSPMESSAGE__INIT;
                                      mg.connid = conid;
                                      mg.seqnum = seq;
                                      mg.payload.data = malloc(output_iterator->msg.payload.len);
                                      mg.payload.len = output_iterator->msg.payload.len;
                                      memcpy(mg.payload.data,payld,data_length);


                                      memset(&buf,0,sizeof(buf));
                                      len=lspmessage__get_packed_size(&mg);
                                      buf=malloc(len);
                                      lspmessage__pack(&mg,buf);

                                      rc = sendto(client_iter->clientfd, buf, len, 0,  (struct sockaddr *)&destination_last_client,  sizeof(destination_last_client));
                                     
                                      //increase the retry count by 1
                                      client_iter->retrycount ++;
                                      //if(client_iter->retrycount>5) 
                                      //     printf("The retry count for connection %d is %d\n",client_iter->ID,client_iter->retrycount); 
                                      free(buf);
                                      free(mg.payload.data);

                                  }


                          }
                   
                  }
            
              pthread_mutex_unlock(&serveroutput);
              pthread_mutex_unlock(&clientlist);
           

              // Task 2 : Acknowledge the most recently received message for each connection

              pthread_mutex_lock(&serverinput);
              pthread_mutex_lock(&clientlist);
           
              lsp_client_list *client_iterate = client_head;
            
           
              int req_seq=1;
         
              struct from_client *input_iterator;
              if(client_iterate->next != NULL) // This shows that there are clients in the Queue
                  {
                      while (client_iterate->next!=NULL)
                          {
                              client_iterate = client_iterate->next;
                                       if(client_iterate->retrycount>5)
                                        continue;
                    
                              destination_last_client = client_iterate->clientaddr;
                      
                              exp_seq_num = client_iterate->expected_seq_num;
                              conid = client_iterate->ID;
              
                              // Now send ACK    

                              LSPMessage mg = LSPMESSAGE__INIT;
                              mg.connid = conid;
                              mg.seqnum = exp_seq_num-1;
                              mg.payload.data = NULL;
                              mg.payload.len = 0;


                              memset(&buf,0,sizeof(buf));
                              len=lspmessage__get_packed_size(&mg);
                              buf=malloc(len);
                              lspmessage__pack(&mg,buf);

                              //printf("\n We are ACking2 the connection for connid %d\n",client_iterate->ID);
                              rc = sendto(client_iterate->clientfd, buf, len, 0,  (struct sockaddr *)&destination_last_client,  sizeof(destination_last_client));
                              free(buf);
                              free(mg.payload.data);
                              //increase the retry count by 1
                              client_iterate->retrycount++;
                                           printf("The retry count for connection %d is %d\n",client_iterate->ID,client_iterate->retrycount); 

                          }
                   
                  }
            
              pthread_mutex_unlock(&serverinput);
              pthread_mutex_unlock(&clientlist);

          

          }
         else if (i>0)
             {
                 //ANKU
                 LSPMessage *message;
                 pthread_mutex_lock(&clientlist);
                 if(FD_ISSET(a_server->sd,&read_set))  
                     {
                         rc = recvfrom(a_server->sd, buffer, sizeof(buffer), 0,(struct sockaddr *)&from, &fromlen);
                         message=lspmessage__unpack(NULL,rc,buffer);
                         
                         if( message->seqnum == 0 && message->connid == 0 && message->payload.len==0)
                             {
                                 int newclientfd = socket(AF_INET, SOCK_DGRAM, 0);
                                 lsp_client_list *client_add = (lsp_client_list *) malloc(sizeof(lsp_client_list));
                                 client_add->ID=globalID;
                                 client_add->clientaddr =*((struct sockaddr_in *)&from);
                                 client_add->next=NULL;
                                 client_add->expected_seq_num = 1;   // Once the client is created, the next message has to start from 1
                                 client_add->expected_seq_num_to_client = 1;  // This is the seq num which the client should expect from server
                                 client_add->clientfd = newclientfd;
                                 //Set the retry count to 0
                                 client_add->retrycount = 0;
                                 struct sockaddr_in destination_cli=*((struct sockaddr_in *)&from);
                              
                                 lsp_client_list *iterator;
                                 iterator = client_head;
                                 if(iterator->next !=NULL)
                                     {
                                         while(iterator->next !=NULL)
                                             {
                                                 iterator = iterator->next;
                                             }
                                     }
                                 iterator->next=client_add;
                                 // We will change the message connid id to the assigned ID , so that the ACK goes with this connection ID
                                 message->connid = globalID;
                                 globalID++;
                              
                              
                                 LSPMessage destination_messag = LSPMESSAGE__INIT;
                                 destination_messag.connid = message->connid;
                                 destination_messag.seqnum = message->seqnum;
                                 destination_messag.payload.data = NULL;
                                 destination_messag.payload.len = 0;
                                 memset(&buf,0,sizeof(buf));
                                 len=lspmessage__get_packed_size(&destination_messag);
                                 buf=malloc(len);
                                 lspmessage__pack(&destination_messag,buf);
                                 rc = sendto(client_add->clientfd, buf, len, 0,  (struct sockaddr *)&destination_cli,sizeof(destination_cli));
                                 //set fd in master fdset
                                 FD_SET(client_add->clientfd,&master_fd_set);
                             }
            
             
                     }
                 else
                     {// RUn a loop over all clients and check if their fd is set
                         
                         lsp_client_list* it = client_head;
                         while(it->next!=NULL)
                             {//client loop
                                 it = it->next;
                                       if(it->retrycount>5)
                                        continue;
                                 if(FD_ISSET(it->clientfd,&read_set)){//client fd
                                     
                                     rc = recvfrom(it->clientfd, buffer, sizeof(buffer), 0,(struct sockaddr *)&from, &fromlen);
                                     //Set the retry count to 0
                                     it->retrycount = 0;
                                     message=lspmessage__unpack(NULL,rc,buffer);
                                     //printf("\nMessage received from client : %s with connid %d seq %d\n",message->payload.data,message->connid,message->seqnum) ; 
                                     // printf("\nMessage received conn %d seq %d length %d\n",message->connid,message->seqnum,message->payload.len) ;           
                                     
                                     if (message->payload.len !=0)     // Handling Message which is coming
                                         {
                                             
                                             //printf("\nMessage received from client : %s with connid %d seq %d\n",message->payload.data,message->connid,message->seqnum) ; 
                                             struct sockaddr_in client_destination = *((struct sockaddr_in *)&from);                                    
                                             
                                             // Send the ACK 
                                             LSPMessage destination_message = LSPMESSAGE__INIT;
                                             destination_message.connid = message->connid;
                                             destination_message.seqnum = message->seqnum;
                                             destination_message.payload.data = NULL;
                                             destination_message.payload.len = 0;
                                             len=lspmessage__get_packed_size(&destination_message);
                                             buf=malloc(len);
                                             lspmessage__pack(&destination_message,buf);
                                             rc = sendto(it->clientfd, buf, len, 0,  (struct sockaddr *)&client_destination,sizeof(client_destination));
                 
                                             // Add this message to input queue, if it is the expected sequence number 
                                             
                                             pthread_mutex_lock(&serverinput);
                                             int id = message->connid;
                                             int expected_sequence_number;
                                             expected_sequence_number = it->expected_seq_num;
                                         
                                             // printf ("\n Expected Seq Number is %d .... Message seq num is %d",expected_sequence_number,message->seqnum);              
                                             
                                             if(expected_sequence_number == message->seqnum)
                                                 {                 
                                                 
                                                     //  printf("\n We will add the message to Server Input Queue now");
                                                     
                                                     struct from_client *from_client_node = (struct from_client *)malloc(sizeof(struct from_client));
                                                 
                                                     from_client_node->msg.connid = message->connid;
                                                     from_client_node->msg.seqnum = message->seqnum;
                                                     
                                                     from_client_node->msg.payload.data = malloc(message->payload.len);
                                                     from_client_node->msg.payload.len = message->payload.len;
                                                     memcpy(from_client_node->msg.payload.data, message->payload.data, message->payload.len);
                                                 
                                                     from_client_node->next = NULL;
                                                     
                                                     struct from_client *from_client_iterator;
                                                     from_client_iterator = from_client_head;
                                                 
                                                     if(from_client_iterator->next != NULL)
                                                         {
                                                             while(from_client_iterator->next != NULL)
                                                                 {
                                                                     from_client_iterator = from_client_iterator->next;
                                                                 }
                                                         }
                                                     
                                                     from_client_iterator->next = from_client_node;
                                                 
                                                     it->expected_seq_num++;   // Since the data got added we need to increment the expected sequece number 
                                                     free(buf);
                                                     free(message->payload.data);
                                                 }
                                         
                                             pthread_mutex_unlock(&serverinput);
                                         }
                                     else   // Handling ACKS
                                         {
                                             
                                         // We will search for the corresponding message in output queue and delete it 
                                         
                                             
                                             pthread_mutex_lock(&serveroutput);
                                             
                                             struct to_client *to_iterator;
                                             to_iterator = to_client_head;
                                             struct to_client *temporary;
                                             temporary = to_client_head;
                                             bool flag = true;
                                             
                                             
                                             //printf("\n Trying to delete output queue");
                                             if(to_iterator->next !=NULL)
                                                 {
                                                     
                                                     while(to_iterator->next != NULL && flag)
                                                         {
                                                             temporary = to_iterator;
                                                             to_iterator = to_iterator->next;
                                                             
                                                             if(to_iterator->msg.connid == message->connid)
                                                                 {
                                                                     // printf("iterator_seqnum :%d  message:%d",to_iterator->msg.seqnum,message->seqnum);
                                                                     if( to_iterator->msg.seqnum == message->seqnum)
                                                                         {
                                                                             temporary->next = to_iterator->next;
                                                                     //printf("\n We are deleting the message with sequence number %d\n",message->seqnum);
                                                                             free(to_iterator);
                                                                             flag = false;
                                                                         }
                                                                 }
                                                         }
                                                 }
                                             
                                             pthread_mutex_unlock(&serveroutput);
                                             
                                         }
                                     
                                 }
                                 //Send ack irrespective of fd being set or not
                     
                                 LSPMessage dest_message = LSPMESSAGE__INIT;
                                 dest_message.connid = it->ID;
                                 dest_message.seqnum = it->expected_seq_num-1;
                                 dest_message.payload.data = NULL;
                                 dest_message.payload.len = 0;
                                 len=lspmessage__get_packed_size(&dest_message);
                                 buf=malloc(len);
                                 lspmessage__pack(&dest_message,buf);
                                 rc = sendto(it->clientfd, buf, len, 0,  (struct sockaddr *)&it->clientaddr,sizeof(it->clientaddr));
                
                                 
                             // Always iterate over output queue to send data to clients  
                 
                             
                             pthread_mutex_lock(&serveroutput);
                             struct to_client *to_client_iter = to_client_head;
                             bool flag= true;
                             struct sockaddr_in destination;
                             while(to_client_iter->next !=NULL)
                                 {
                                     //printf("\n We are into server output queue");
                                     to_client_iter = to_client_iter->next;
                                     
                                     if ( to_client_iter->msg.connid == it->ID)
                                         {
                                             destination = it->clientaddr;
                                             //printf("\n The connid to whom we will send is %d",client_match->ID);
                                             flag=false;
                                             LSPMessage mgg = LSPMESSAGE__INIT;
                                             mgg.connid = to_client_iter->msg.connid;
                                             mgg.seqnum = to_client_iter->msg.seqnum;
                                             mgg.payload.data = malloc(to_client_iter->msg.payload.len);
                                             mgg.payload.len = to_client_iter->msg.payload.len;
                                             memset(mgg.payload.data,0,mgg.payload.len);
                                             memcpy(mgg.payload.data,to_client_iter->msg.payload.data, to_client_iter->msg.payload.len);
                                             //printf("\nOUTPUT: Data to be sent is %s to client with id %d\n",to_client_iter->msg.payload.data,mgg.connid);
                                             memset(&buf,0,sizeof(buf));
                                             len=lspmessage__get_packed_size(&mgg);
                                             buf=malloc(len);
                                             lspmessage__pack(&mgg,buf);
                                             //printf("\n Server is sending message : %s to %d\n sequence number : %d\n",mgg.payload.data,mgg.connid,mgg.seqnum);
                                             if(prob>drop_rate){
                                              //printf("Sending to client%f>%f\n",prob,drop_rate);
                                                 rc = sendto(it->clientfd, buf, len, 0,  (struct sockaddr *)&destination, sizeof(destination));
						}
						 it->retrycount ++;
                                             break;

                                         }
                                 }
                             pthread_mutex_unlock(&serveroutput);
                             }
                     }
                 pthread_mutex_unlock(&clientlist);
             }
        
    }

    return NULL;
}


lsp_server* lsp_server_create(int port)
{
   lsp_server *myserver =(lsp_server *) malloc(sizeof(lsp_server));
   int rc,ts;  
   //struct sockaddr_in ServAddr;
   myserver->sd = socket(AF_INET, SOCK_DGRAM, 0); // Creating the socket
   if(myserver->sd>0)
       {
           printf("\n Socket created successfully");
       }
   
   // Clearing of Bits and setting it to 0 at myserver->ServAddr
   memset(&myserver->ServAddr, 0, sizeof(myserver->ServAddr)); 
   
   // Filling in Data for ServerAddress
   myserver->ServAddr.sin_family      = AF_INET;
   myserver->ServAddr.sin_port        = htons(port);
   myserver->ServAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
      
   // We will bind the socket to the address	
   rc=bind(myserver->sd, (struct sockaddr *)&myserver->ServAddr, sizeof(myserver->ServAddr));
   if (rc<0)
   {
      printf("\n Bind UnSuccessful at %d",port);
   }
   else if ( rc==0)
   {
      printf("\n Bind Successfull");
   }
   
   // We will initialize all the queues here , so that we can start to use them
   init_server_queues();
   pthread_mutex_init(&serveroutput, NULL);
   pthread_mutex_init(&serverinput,NULL);
   pthread_mutex_init(&clientlist,NULL);
   

   ts=pthread_create(&server_epoch_thread,NULL,serverEpoch,(void*)myserver);
   assert(0==ts);
 

   if(rc==0)
   {
     return myserver;
   }
   
}

int init_server_queues()
{
    pthread_mutex_lock(&clientlist);
    lsp_client_list *client_new = (lsp_client_list *) malloc(sizeof(lsp_client_list));
    client_new->ID=0;
    client_new->next=NULL;
    client_head=client_new;
    pthread_mutex_unlock(&clientlist);

   
    pthread_mutex_lock(&serverinput); 
    struct from_client *from_client_new = (struct from_client *)malloc(sizeof(struct from_client));
    LSPMessage msg = LSPMESSAGE__INIT;
    from_client_new->msg=msg;
    from_client_new->next = NULL;
    from_client_head = from_client_new;
    pthread_mutex_unlock(&serverinput);
    
    pthread_mutex_lock(&serveroutput);
    struct to_client *to_client_new = (struct to_client *)malloc(sizeof(struct to_client));
    LSPMessage msg1 = LSPMESSAGE__INIT;
    to_client_new->msg=msg1;
    to_client_new->next = NULL;
    to_client_head = to_client_new;
    pthread_mutex_unlock(&serveroutput);
    
    return 0;
}





int lsp_server_read(lsp_server* a_srv, void* pld, uint32_t* conn_id)
{
   
   
   struct from_client *from_client_iterator;
   struct from_client *temporary;
   int byte=0;
   temporary = from_client_head;

   pthread_mutex_lock(&serverinput);
   from_client_iterator = from_client_head;
   
   if(from_client_iterator->next !=NULL)
   {
   from_client_iterator = from_client_head->next;

   
   memcpy(pld,from_client_iterator->msg.payload.data,from_client_iterator->msg.payload.len);
   memcpy(conn_id,&from_client_iterator->msg.connid,sizeof(from_client_iterator->msg.connid));   
      
   //printf("\nMessage from client %d is %s\n",from_client_iterator->msg.connid,from_client_iterator->msg.payload.data);

   byte = from_client_iterator->msg.payload.len;
   

   temporary->next = from_client_iterator->next;
   free(from_client_iterator);

   

   }

   pthread_mutex_unlock(&serverinput);

   
   return byte;
  
}


bool lsp_server_write(lsp_server* a_srv, void* pld, int lth, uint32_t conn_id)
{
   
    // printf ("\n We are in SERVER WRITE\n");   
    bool stable = false;
    if ( conn_id !=0)
        {   
            
            pthread_mutex_lock(&clientlist);
            pthread_mutex_lock(&serveroutput);
            
            bool flag = true;
            LSPMessage output_queue_message = LSPMESSAGE__INIT;
            output_queue_message.connid = conn_id;  // We will have to decide what it to be put for both connection id and sequence number
            
            // Wec will iterate over the client list to know the exected sequence number for this client


            lsp_client_list *match_client = client_head;
            if(match_client->next !=NULL)
                {
                    
                    while (match_client->next!=NULL && flag)
                        {
                            match_client = match_client->next;
                            
                            if ( match_client->ID == conn_id && match_client->retrycount<5)
                                {
                                    stable = true;
                                    output_queue_message.seqnum = match_client->expected_seq_num_to_client;
                                    match_client->expected_seq_num_to_client+=1;  // The next expected sequence number has to be increaesd
                                    flag=false;
                                }
                        }
                    if(stable)
                        {
                            output_queue_message.payload.data = malloc(sizeof(uint8_t) * lth);
                            output_queue_message.payload.len = sizeof(uint8_t)*lth;
                            memset(output_queue_message.payload.data,0,sizeof(uint8_t)*lth);
                            memcpy(output_queue_message.payload.data, pld, lth*sizeof(uint8_t));
                            
                            //printf("\n The message from client %d  has seqnum %d\n",output_queue_message.connid,output_queue_message.seqnum);
                            
                            
                            // Add this message to the server_output queue
                            struct to_client *node = (struct to_client *)malloc(sizeof(struct to_client));
                            node->msg.connid = output_queue_message.connid;
                            node->msg.seqnum = output_queue_message.seqnum;
                            node->msg.payload.data = malloc(output_queue_message.payload.len);
                            node->msg.payload.len = output_queue_message.payload.len;
                            memcpy(node->msg.payload.data, output_queue_message.payload.data, output_queue_message.payload.len);
                            node->next = NULL;
     
                            struct to_client *to_client_iterator = to_client_head;
                            if(to_client_iterator->next !=NULL)
                                {
                                    while(to_client_iterator->next !=NULL)
                                        {
                                            to_client_iterator = to_client_iterator->next;
                                        }
                                }
                            to_client_iterator->next = node;
                            
                        }
                    else
                        printf("Connection for conn id %d is closed\n",match_client->ID);
                }
            pthread_mutex_unlock(&clientlist);
            pthread_mutex_unlock(&serveroutput);
        }
    if(stable)    
        return true; 
    else 
        printf("Retry count is greater than 5 for client\n");
        return false;
}

bool lsp_server_close(lsp_server* a_srv, uint32_t conn_id)
{
    pthread_kill(server_epoch_thread,0);
    lsp_client_list *client_iterate= client_head;
    lsp_client_list *previous = client_head;
    if(client_iterate!=NULL)
        {
            while(client_iterate!=NULL&&client_iterate->ID!=conn_id){
                previous = client_iterate;
                client_iterate = client_iterate->next;

            }

        }
    previous->next = client_iterate->next;
    free(client_iterate);
}


