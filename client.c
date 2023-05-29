//Gabriele Iacovacci 0299026 
//Progetto Sistemi Operativi 2022/2023


#include <sys/socket.h>
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#include <netdb.h>

#define BUFSIZE 4096
#define ERROR (-1)

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;
typedef struct sigaction S_ACT;


void 	handle_intr(int dummy,siginfo_t* dummy1, void* dummy2);
void 	sigpipe_handler(int dummy, siginfo_t* dummy1, void* dummy2);
int 	check (int exp, const char* error);
int* 	port_grab(int argc, char** argv, int* p_port);
char** 	addr_grab(int argc, char** argv, char** p_addr);


int main(int argc, char* argv[]){

	// before initializing anything make sure to ave at least the address, port may be set on default
	int 	SERVERPORT = 4444;
	char 	*s_addr = malloc(sizeof(char)*256);

	if(addr_grab(argc, argv, &s_addr)==NULL) exit(0);

	port_grab(argc, argv, &SERVERPORT);

	//argv is alright so init some variables
	int 	addr_size, my_socket;
	char 	buffer[BUFSIZE] = { 0 };
	char* 	ex = "exit\n";
	SA_IN 	server_addr;
	S_ACT 	action={ 0 };
	S_ACT 	action2={ 0 };
	struct 	timeval timer;
	struct	  hostent *he;

	addr_size = sizeof(SA_IN);
	check((my_socket = socket(AF_INET, SOCK_STREAM, 0))
			,"Failed to create socket");

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons((long)SERVERPORT) ;

	//check(inet_aton(s_addr, &server_addr.sin_addr),"Bad IP address");

	if ( inet_aton(s_addr, &server_addr.sin_addr) <= 0 ){
		printf("client: indirizzo IP non valido.\nclient: risoluzione nome...");
		
		if ((he=gethostbyname(s_addr)) == NULL)
		{
			printf("fallita.\n");
  			exit(EXIT_FAILURE);
		}
		printf("riuscita.\n\n");
		server_addr.sin_addr = *((struct in_addr *)he->h_addr);
    }

	check(connect(my_socket, (SA*)&server_addr, (socklen_t)addr_size ),"Connessione non stabilita, il server potrebbe essere offline...");

	// enable reciving ^C, the function handle_intr will handle ^C 
	action2.sa_sigaction = &handle_intr;
	sigaction(SIGINT,&action2,NULL);

	// set the timer at 5 sec
	timer.tv_sec = 10;
	timer.tv_usec = 0;

	// create a set of fds and put the socket fd in it 
	fd_set fds ;		
	FD_ZERO(&fds);
	FD_SET( my_socket, &fds);



    while(1){

		check(read(my_socket, buffer, BUFSIZE), "Connessione interrotta! La sessione del server potrebbe essere scaduta...");

		// check if the message from server has lenght >0
		if(strlen(buffer)>0){

			// if server sends "exit\n" means that an exit request has been sent
			if(!strcmp(buffer,"exit\n")){
				printf("Connection closed...\n");
				close(my_socket);
				exit(0);
			}

			//chat string
			printf("\033[0;31mserver\033[0m >> ");
			fflush(stdout);
			
			// when sendig files(that contain larger data then a buffer), the server will start by sending "/* "
			// and finish with a "*/"
			if((buffer[0]=='/') && (buffer[1]=='*')){
				size_t  size=0;
				int c;

				int flags = fcntl(my_socket, F_GETFL, 0);
				fcntl(my_socket, F_SETFL, flags | O_NONBLOCK);


				
				while(select(my_socket+1, &fds,NULL, NULL ,&timer) && size!=-1){
					memset(buffer,0,BUFSIZE);
					size = read(my_socket, buffer, BUFSIZE);

					//read a sort of ack then stop
					if((buffer[strlen(buffer)-2]=='*') && (buffer[strlen(buffer)-1]=='/')){
						buffer[strlen(buffer)-2] = '\0';
						printf("%s",buffer);
						break;
					}

					printf("%s",buffer);
					fflush(stdout);	
				}

				if (size==0){
					printf("Invio del backup non riuscito...\n");
				}

				fcntl(my_socket, F_SETFL, flags);
				memset(buffer,0,BUFSIZE);
			}else{
				printf("%s\n", buffer);
			}

			//chat string
			printf("\033[0;34myou\033[0m >> ");

    		memset(buffer,0,BUFSIZE);
    		fgets(buffer, BUFSIZE, stdin);
    		check(write(my_socket, buffer, BUFSIZE), "Connessione interrotta! La sessione del server potrebbe essere scaduta...");
    		memset(buffer,0,BUFSIZE);
		}
		else{
			printf("Il server non risponde, dopo un' inattivita di 60s la sessione viene terminata...");
			break;
		}
    }
    return 0;
}

	
int check(int exp, const char* error){
	if(exp < 0 ){
		printf("%s\n",error);
		exit(-1);
	}
	return 0;
	
}

int* port_grab(int argc, char** argv, int* p_port){
	if (argc<4){
		return NULL;
	}
	else if (argc == 4){
		printf("Sintax:\n\tclient -a (server address) [-p] (port number) \n%i\n", argc);
		return NULL;
	}
	if (argc>4 && (strncmp(argv[3] ,"-p",2) || strncmp(argv[3] ,"-P",2))){
		*p_port = strtol(argv[4], NULL, 0);
		return p_port;
	}
	return NULL;
}

char** addr_grab(int argc, char** argv, char** p_addr){
	 if (argc <= 2){
		printf("Sintax:\n\tclient -a (server address) [-p] (port number)\n\n");
		return NULL;
	}else if (strncmp(argv[1] ,"-a",2) || strncmp(argv[1] ,"-A",2)){
		strcpy(*p_addr,argv[2]);
		return p_addr;
	}
	return NULL;
}

void 	handle_intr(int dummy,siginfo_t* dummy1, void* dummy2){
	printf("Chiusura insapettata, disconnessione avvenuta.\n\t\033[0;33mPer effetuare una disconnessione correttamente digitare \"exit\" oppure \"e\".\n\033[0m");
	fflush(stdout);
	exit(0);
}

void 	sigpipe_handler(int dummy,siginfo_t* dummy1, void* dummy2){
	printf("Il server ha terminato la connessione, per riconnetere rieseguire il client...\n");
	fflush(stdout);
	exit(0);	
}

