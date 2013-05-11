#include "lsp.h"
#include<stdio.h>
#include<openssl/sha.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LENGTH 4096

char* global_pld;

char * increment (char *string, int length)
{
    char temp[length];
    
    int i = length-1;
    strcpy(temp,string);
    char *newstring = (char*)malloc(2);
    while(i>=0)
        {
            if(temp[i]=='z')
                {
                    temp[i] = 'a';
                    i = i-1;
                }
            else 
                {
                temp[i]=temp[i]+1;
                strcpy(newstring, temp);
                return newstring;
                }
        }

}



bool handle_req(char *pld,int bytes)
{
 
   char* ch=pld;
   

   int i = 0;
   char *password,*upper;
   char *compare_hash;
   char *msg;
   char converted[21];
   size_t length;
   int count=0;
   int len=0;
   unsigned char hash[SHA_DIGEST_LENGTH];
   bool found = false;

   char *s;
   s = malloc(bytes+1);
   memset(s,0,bytes+1);
   memcpy(s,pld,bytes);

   char* token = strtok(s, " ");
   
   while (token)
   {
    count++;
    
    if( count == 2)
    {
      compare_hash = malloc(strlen(token)+1);
      memset(compare_hash,0,strlen(token)+1);
      memcpy(compare_hash,token,strlen(token));
    }
    if(count == 3)
    {
      
      password = malloc(strlen(token)+1);
      memset(password,0,strlen(token)+1);
      memcpy(password,token,strlen(token));
      length = strlen(token);
    }
     if(count == 4)
    {
      
      upper = malloc(strlen(token)+1);
      memset(upper,0,strlen(token)+1);
      memcpy(upper,token,strlen(token));
      //length = strlen(token);
    }
    token = strtok(NULL, " ");
   }

   
   if(ch[0]=='c')
   {
       printf("\nStarting to crack password\n");

       while(strcmp(password,upper)!=0 && !found)
       {

                   
            SHA1(password,length,hash);
            for(i=0;i<20;i++)
            {
                sprintf(&converted[i*2],"%02x",hash[i]);
            }
            if(strcmp(converted,compare_hash)==0)
	    {
               printf("\nThe password is %s\n",password);
               found=true;
               break;
            }
            password=increment(password,length);
                        
       }

       

       // Generate the Packet which is to be sent
       if(found)
       { 
          len=2+strlen(password);
          char string[len];
          string[0]='f';
          string[1]=' ';
          for(i=0;i<strlen(password);i++)
          {
             string[i+2]=password[i];
          }      
          
          
          msg=malloc(len);
          memset(msg,0,len);
          memcpy(msg,string,len);
       }
       else if(!found)
       {
          char strng[1];
          strng[0]='x';
          len=1;
          
          msg=malloc(len);
          memset(msg,0,len);
          memcpy(msg,strng,len);

          printf("\nPassword Not Found\n");
          
       }
       

       global_pld = malloc(len);
       memset(global_pld,0,len);
       memcpy(global_pld,msg,len);
       
   }

   return true;   
}



int main(int argc, char** argv)
{

 lsp_set_drop_rate(0.5);  
  if(argc == 2)
    {
//    srand(12345);

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

      //        printf("%d %d %d\n",ii,j,k);
    }


        
    lsp_client* myworker = lsp_client_create(host,atoi(port));
     if (myworker==NULL)
	{
	  printf("Worker not created\n");
     	  return true;
      }
     char message[] ="j";
    lsp_client_write(myworker, (void *) message, strlen(message));
    for(;;)
    {
         //lsp_client_write(myworker, (void *h) message, strlen(message));
         uint8_t buffer[4096];
         memset(buffer,0,4096);
         int bytes_read = lsp_client_read(myworker,buffer);
         sleep(2);
	//printf("Received bytes %d \n",bytes_read);
         if(bytes_read >0 )
         {
            //printf("Received bytes %d \n",bytes_read);
	    if(buffer[0]!='c')
		continue;
	    handle_req(buffer,bytes_read);
            lsp_client_write(myworker, (void *) global_pld, strlen(global_pld));
         }
         else if (bytes_read<0) {
             printf("Disconnected\n");
             break;
         }
    
    }

    lsp_client_close(myworker);
    //    printf("End of worker code\n");
    
    }
    else
    {
       printf("\nPlease enter correct number of arguments\n");
    }
    return 0;
} 
