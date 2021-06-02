#include <pthread.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "list.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#define MAXBUFLEN 4000

struct addrinfo hints, *servinfo;
struct addrinfo houts, *servinfo2;
struct hostent *hosto;

pthread_t keyboard;
pthread_t sending;
pthread_t listener;
pthread_t print;

pthread_mutex_t mutexreceive, mutexsend;
pthread_cond_t conddisplay, condsend;

int sockfd;
int testSock;
int rv;
int rvo;
List *sender;
List *receiver;

int check = 1;
char* encrypt(char *tmp)
{
	for(int i=0;i<strlen(tmp);i++)
	{
		tmp[i] +=32;
		tmp[i]%=256;
	}
	return tmp;
}
char* decrypt(char* tmp)
{
	for(int i=0;i<strlen(tmp);i++)
	{
		tmp[i] -=32;
		if(tmp[i]<0)
			tmp[i]+=256;
	}
	return tmp;
}
char *strremove(char *str, const char *sub) {
    size_t len = strlen(sub);
    if (len > 0) {
        char *p = str;
        while ((p = strstr(p, sub)) != NULL) {
            memmove(p, p + len, strlen(p + len) + 1);
        }
       //   free(p);
    }
  
    return str;
}
void *keyboardThread(void * args)
{
	keyboard = pthread_self();
	char buf[MAXBUFLEN];
	char* temp;
	while(check)
	{
		temp = readline("");
		strcpy(buf,temp);
		free(temp);
		strcat(buf,"\n");
		temp = readline("");
		while(strlen(temp) != 0)
		{
			if(temp == NULL)
			{
				free(temp);
				break;
			}
			strcat(buf, temp);
			free(temp);
			strcat(buf,"\n");
			temp = readline("");
		}
		free(temp);
		if(strstr(buf,"!exit\n")!=NULL)
		{	
			pthread_mutex_lock(&mutexsend);
			//strcpy(buf, (char*) encrypt(buf));
			int i = sendto(sockfd, encrypt(buf), sizeof(buf), 0, servinfo2->ai_addr, servinfo2->ai_addrlen);	
			check = 0;
			pthread_mutex_unlock(&mutexsend);
			pthread_mutex_unlock(&mutexreceive);
			pthread_cancel(sending);
			pthread_cancel(listener);
			pthread_cancel(print);
			free(temp);
			freeaddrinfo(servinfo);
			freeaddrinfo(servinfo2);
			List_free(sender,NULL);
			List_free(receiver,NULL);
			pthread_exit(NULL);
			
		}
		if(strstr(buf,"!status\n")!=NULL)
		{
			testSock = socket(servinfo2->ai_family, servinfo2->ai_socktype, servinfo2->ai_protocol);
			if(bind(testSock, servinfo2->ai_addr, servinfo2->ai_addrlen)!=-1)
			{
				printf("offline\n");
				close(testSock);
			}
			else
				printf("online\n");
			strremove(buf,"!status\n");
		}
		pthread_mutex_lock(&mutexsend);
		List_add(sender,buf);
		pthread_cond_signal(&condsend);
		pthread_mutex_unlock(&mutexsend);
		
	}	
}

void *senderThread(void *args)
{
	sending = pthread_self();
	char send[MAXBUFLEN];
	while(check){
		pthread_mutex_lock(&mutexsend);
		if(List_count(sender) == 0)
			pthread_cond_wait(&condsend, &mutexsend);
		while(List_count(sender) > 0){
			strcpy(send, (char*) List_first(sender));
			List_remove(sender);
			//strcpy(send, (char*) encrypt(send));
			int i = sendto(sockfd, encrypt(send), sizeof(send), 0, servinfo2->ai_addr, servinfo2->ai_addrlen);
			}
		pthread_mutex_unlock(&mutexsend);
	}
}
void *listenerThread(void *args)
{
	listener = pthread_self();
	char get[MAXBUFLEN];
	while(check){
		while(recvfrom(sockfd, get, sizeof(get), 0, servinfo2->ai_addr, &(servinfo2->ai_addrlen))!=-1){
			pthread_mutex_lock(&mutexreceive);
			List_add(receiver, get);
			//printf("%s", get);
			fflush(stdout);
			pthread_cond_signal(&conddisplay);
			pthread_mutex_unlock(&mutexreceive);
		}			
	}
}

void *printThread(void* args)
{
	print = pthread_self();
	char msg[MAXBUFLEN];
	while(check){
		pthread_mutex_lock(&mutexreceive);
		if(List_count(receiver)==0)
			pthread_cond_wait(&conddisplay, &mutexreceive);
		while(List_count(receiver) > 0){
			strcpy(msg, (char*) List_first(receiver));
			List_remove(receiver);
			decrypt(msg);
			if(strstr(msg,"!exit\n")!=NULL)
			{
				check = 0;
				printf("%s",msg);
				pthread_mutex_unlock(&mutexsend);
				pthread_mutex_unlock(&mutexreceive);
				pthread_cancel(keyboard);
				pthread_cancel(sending);
				pthread_cancel(listener);
				pthread_exit(NULL);
				//exit(0);
			}
			printf("%s",msg);	 
		}
		pthread_mutex_unlock(&mutexreceive);
	}
}

 int main (int argc, char *argv[])
 {
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	
	memset(&houts, 0, sizeof houts);
	houts.ai_family = AF_UNSPEC;
	houts.ai_socktype = SOCK_DGRAM;
	

	
	if (pthread_mutex_init(&mutexsend, NULL) != 0){
		perror("listenr: sending mutex");
	}
	if(pthread_mutex_init(&mutexreceive, NULL) != 0){
		perror("listner: receiving mutex");
	}
	
	if (argc != 4) {
		fprintf(stderr,"usage: lets-talk [my port number] [remote machine name] [remote port number]  \n");
		exit(1);
	}
	
	if((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	if((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1){
		perror("listener: socket");
	} 
	
	if(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1){
		close(sockfd);
		perror("listener: bind");
	}
	
	 hosto= gethostbyname(argv[2]);
	
	if((rvo = getaddrinfo(inet_ntoa(*(struct in_addr*)hosto->h_addr_list[0]), argv[3], &houts, &servinfo2)) !=0){
		fprintf(stderr, "getaddrinfo (out): %s\n", gai_strerror(rvo));
		return 1;
	}
	if (pthread_mutex_init(&mutexsend, NULL) != 0){
		perror("listenr: sending mutex");
	}
	if(pthread_mutex_init(&mutexreceive, NULL) != 0){
		perror("listner: receiving mutex");
	}

	sender= List_create();
	receiver=List_create();
	
    
	pthread_t t1, t2, t3, t4;
	
	int tt1, tt2, tt3, tt4;
	
	tt1 = pthread_create( &t1, NULL, printThread, NULL);
	tt2 = pthread_create( &t2, NULL, keyboardThread, NULL);
	tt3 = pthread_create( &t3, NULL, senderThread, NULL);
	tt4 = pthread_create( &t4, NULL, listenerThread, NULL);

	pthread_join( t1, NULL);
	pthread_join( t2, NULL);
	pthread_join( t3, NULL);
	pthread_join( t4, NULL);

	
    	close(sockfd);
    	return 0;
  
 }
