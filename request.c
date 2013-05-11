#include "lsp.h"
#include <pthread.h>
#include <time.h>
#include<string.h>


bool handle_req(char *pld,int bytes)
{
     char* ch=pld;
     int count=0;
     char *password;
     char *s;
     s = malloc(bytes);
     memset(s,0,bytes);
     memcpy(s,pld,bytes);


     if(ch[0]=='f')
     {
         char* token = strtok(s, " ");

         while (token)
         {
            count++;
            
            if( count == 2)
            {
                 password = malloc(strlen(token));
                 memset(password,0,strlen(token));
                 memcpy(password,token,strlen(token));
            }
            
            token = strtok(NULL, " ");
         }

         printf("Found: %s\n",password);
         return true;
      
     }
     else if(ch[0]=='x')
     {
        printf("Not Found\n");
        return true;
     }
     
     return false;   
}


int main(int argc, char** argv) 
{
	
    lsp_set_drop_rate(0);
    if(argc == 4)
    {

    char *host_port = argv[1];
    
    char host[20];
    char port[7];
    bool flg=false;
    int j=0;
    int k=0;
    int ii=0; 
    memset(host,0,20);
    memset(port,0,7);

    for(;ii<strlen(argv[1]);ii++)
    {
      if(host_port[ii] == ':')
         flg=true;
      if(flg==true && host_port[ii]!=':')
          port[j++]=host_port[ii];
      if(flg == false && host_port[ii]!=':')
          host[k++]=host_port[ii]; 

      //	printf("%d %d %d\n",ii,j,k);
    }
   
    // printf("%s %s\n",host,port);

    lsp_client* myclient = lsp_client_create(host,atoi(port));
	if(myclient == NULL){
		printf("Server Not Avaialble\n");
		return 0;
	}
    // printf("%s %s connected\n",host,port);

    // We will generate the Crack Request here
	
    char message[] = "c ";
    char *lower;
    char *upper;
    
    //char* message1 = strcat(message,argv[2]);
    char* message1;
    int ll=atoi(argv[3])+1;
    int i=0;
    lower=malloc(ll);
    upper=malloc(ll);
    memset(lower,0,ll);
    memset(upper,0,ll);

    int msg_len = 1+1+strlen(argv[2])+1+ll-1+1+ll-1+1;
    char *send_message = malloc(msg_len);
    memset(send_message,0,msg_len);

    for(i=0;i<(ll-1);i++)
        {
            lower[i]='a';
            upper[i]='z';
        }            
    
    sprintf(send_message,"c %s %s %s",argv[2],lower,upper);
    message1=send_message;
        
    lsp_client_write(myclient, (void *) message1, strlen(message1));
     
    flg = false;
    while(!flg)
    {    
       if(myclient==NULL)
                printf("Client not created\n");
	
       uint8_t buffer[4096];
       memset(buffer,0,4096);
       int bytes_read = lsp_client_read(myclient, buffer);
       
       if(bytes_read > 0)
       {
          //printf("Bytes read are %d\n",bytes_read);
           flg=handle_req(buffer,bytes_read);
          
          
       }
       
       else if (bytes_read<0) {
           printf("Disconnected\n");
           break;
       }
       //lsp_client_close(myclient);
    }
	printf("Client Exiting\n");

    
 }
 else
 {
     printf("\nPlease enter correct number of command line arguments\n");
 }
  
  return 0;
}
