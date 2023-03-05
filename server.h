//Gabriele Iacovacci 0299026 
//Progetto Sistemi Operativi 2022/2023


#include <sys/socket.h>
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

//--macro section--
#define BUFSIZE 4096
#define SERVER_BACKLOG 10
#define MAXACCOUNTS 100
#define MAXONLINE 50
#define TIMER 60


//--type definition section--
typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;
typedef struct sigaction S_ACT;
typedef struct client client;
typedef struct adminreport REPORT;
typedef struct server ONLINE;

struct client{
	int ptr;
	SA_IN addr;
};

struct adminreport{
	int code;
	client* references;
	char user[17];
	REPORT* next;
};

struct server {
	int users;
	int fds[MAXONLINE];
};


//--global variables--
bool	maintenance = false;
int 	SERVERPORT = 4444;
int 	server_socket, key, shmid, dim;

REPORT* head;
ONLINE* currsession;

pthread_t 	reporter;
pthread_mutex_t logmutex;

//--thread variables--
__thread 	S_ACT 	action = { 0 };
__thread 	SA_IN 	server_addr, client_addr;
__thread	REPORT 	*data;
__thread	int 	file, file2, size, fd, count;
__thread	char* 	user, *ca, *dest;
__thread	char 	dir[256],dir2[256],temp1[BUFSIZE],temp2[BUFSIZE], *buffer;
__thread 	sigset_t set;
__thread 	FILE * 	ptr;

//check if the calling function returns an error, if so closes the connection and sends a signal to awake the reporter
int check(int exp, const char* error, int fd){
	if(exp < 0){
		printf("%s",error);
		perror(">");
		fflush(stdout);
		if (fd!=0){
			currsession->users --;
			pthread_mutex_lock(&logmutex);
			if (data->code != 3)data->code = -1;
			pthread_mutex_unlock(&logmutex);
			pthread_kill(reporter, SIGUSR1);
			close(fd);
		}
		pthread_exit(0);
	}
	return 0;
}

//expects fd to be a tcp socket, ask for a sentences in "buffer" reads the answer and puts it in "buffer";
int ask(int fd, char** buffer){
	char temp[BUFSIZE];
	size_t size = strlen(*buffer);

	
	strcpy(temp,*buffer);
	//check(write(fd,temp, strlen(temp)), ">> Failed to write on socket! Client maybe logged out...\n",fd);
	send(fd, temp, strlen(temp), MSG_NOSIGNAL );


	if (errno == EPIPE || errno == EAGAIN || errno == EWOULDBLOCK){
		pthread_mutex_lock(&logmutex);
		if (data->code != 3)data->code = -1;
		pthread_mutex_unlock(&logmutex);
		pthread_kill(reporter, SIGUSR1);
		close(fd);
		pthread_exit(0);
	}


	memset(temp,0,BUFSIZE);
	memset(*buffer,0,BUFSIZE);

	check(read(fd, temp, BUFSIZE), ">> Failed to read from socket! Client maybe logged out or session expired...\n",fd);

	if (errno == EPIPE || errno == EAGAIN || errno == EWOULDBLOCK){
		pthread_mutex_lock(&logmutex);
		if (data->code != 3)data->code = -1;
		pthread_mutex_unlock(&logmutex);
		pthread_kill(reporter, SIGUSR1);
		close(fd);
		pthread_exit(0);
	}

	temp[strlen(temp)-1]=0;
	strcpy(*buffer, temp);
	return 0;
}


// gets password from a already allocated global char* var named "buffer".
void get_psw(){
	// while cycle to get valid password
	sprintf(buffer,"Imposta una nuova password composta da minimo di 6 o da un massimo di 16 caratteri:");
	while(1){

		ask(fd, &buffer);

		if(strlen(buffer)>=6 && strlen(buffer)<=16){

			//put password in new file psw
			sprintf(dir,"%s/psw",user);
			check(file = open(dir, O_RDWR | O_CREAT, 0666),"Failed to enroll account!",0);
			check(write(file,buffer,strlen(buffer)),"Failed to write password",file);
			close(file);

			printf(">> User <%s> completed registration...\n", user);

			
			sprintf(buffer,"Iscrizione completata! Benvenuto/a %s!",user);
			break;
		}
		sprintf(buffer,"Inserire una password valida...");
	}
}

//if the number the current line "i" is found in the "lines" array returns 1;
int linetodelete(int i, int* lines){
	for(int t = 0 ; t<4096; t++ ){
		if(lines[t] == i){
			return 1;
		}
	}
	return 0;
}

//check for new messages in user/new file and prints them
void check4new(int login){
	//check for new messages in file new, try open file data/user/new
	sprintf(dir,"%s/new",user);

	if ((file = open(dir, O_RDWR, 0666))!=-1){



			memset(temp2,0,BUFSIZE);
			sprintf(temp2,"/*");
			check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",fd);

			sprintf(temp1,"%s Sono presenti dei nuovi messaggi:\n\n",buffer);

			do{
				check(write(fd, temp1, BUFSIZE), "Failed to upload backup!",fd);
				memset(temp1,0,BUFSIZE);
				check(size = read(file, temp1, BUFSIZE), "Failed to upload backup!",fd);

				if(size <= 0){
					if (login == 1){
						sprintf(buffer,"\n\tCos'altro fare?\033[0;33m(Per accedere al menù delle opzioni digitare \"help\")\n\033[0m*/");
						break;
					}
					sprintf(buffer,"\n\tCos'altro fare?\n*/");
					break;
				}
			}while(1);

			printf(">> Uploaded <%s>'s new messages...\n", user);	


			//close file descriptor and delete file new
			close(file);
			remove(dir);
	}else{
		if (login == 1){
			sprintf(buffer,"%s Non ci sono nuovi messaggi. Cos'altro fare?\n\t  \033[0;33m(Per accedere al menù delle opzioni digitare \"help\" o \"h\")\033[0m",buffer);
		}else{
			sprintf(buffer,"%s Non ci sono nuovi messaggi. Cosa desideri fare?",buffer);
		}		
	}
}





//grabs the port number from argv and converts it   
int* port_grab(int argc, char** argv, int* p_port){

	if (argc>2){
		if(strncmp(argv[1] ,"-p",2) || strncmp(argv[1] ,"-P",2)){
			*p_port = strtol(argv[2], NULL, 0);
		
		}
		return p_port;
	}
	else if (argc == 2){
		printf("Sintax:\n\tserver.c [-p] (port number)\n\n");
		return NULL;
	}
	return p_port;
}


void append(REPORT* head, REPORT* tobeadded){
	REPORT* temp = head;

	//lock the list for adding one element
	pthread_mutex_lock(&logmutex);

	while(temp->next != NULL){
		temp = temp->next;
	}
	temp->next = tobeadded;

	pthread_mutex_unlock(&logmutex);
}
void pop(REPORT* tobedeleted){
	REPORT* temp = head;

	//lock the list for popping one element
	pthread_mutex_unlock(&logmutex);

	while(temp != NULL){
		if (temp->next == tobedeleted) temp->next = temp->next->next; 
		else temp = temp->next;
	}
	pthread_mutex_unlock(&logmutex);
}

