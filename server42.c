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

#include "server.h"
/*

//--macro section--
#define BUFSIZE 4096
#define ERROR (-1)
#define SERVER_BACKLOG 10

--type definition section--
typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;
typedef struct sigaction S_ACT;
typedef struct client client;
typedef struct adminreport REPORT;

struct client{
	int ptr;
	SA_IN addr;
};

struct adminreport{
	int code;
	char user[17];
	client* references;
	REPORT* next;
};

*/

/*--functions declaration--*/
void* 	handle_connection(void* p_client_socket);
void 	sigpipe_handler(int dummy, siginfo_t* dummy1 , void* dummy2);
void 	sigusr1_handler(int dummy, siginfo_t* dummy1 , void* dummy2);
int 	check(int exp, const char *error, int c);
int 	ask(int fd, char** buff1);
int* 	port_grab(int argc, char** argv, int* p_port);
int 	linetodelete(int i, int* lines);
void	check4new(int login);
void	get_psw();
void	append(REPORT* head, REPORT* tobeadded);
void	pop(REPORT* tobedeleted);
void*	reporterloop();



int main(int argc, char **argv){	
//	Main thread: after intialing the TCP connection, the status list  and managing some signals.
//  After enters a while(1) accepting clients from the net, retriving their IP and the intended fd, 
//  builds a struct to pass those info to the client's thread.


	pthread_t t;
	char 	buf[BUFSIZE];
	int 	addr_size, client_socket, dir_check, fd, banned = 0;
	client 	*p_client;

	currsession = malloc(sizeof (ONLINE));
	currsession->users = 0;


	// if -p is set grab the defined port in argv[2]
	if(port_grab(argc, argv, &SERVERPORT)==NULL){
		exit(0);
	}

	//malloc some space for the head of the list for the sessions
	head = malloc(sizeof(REPORT));
	head->code=0;
	head->next = NULL;


	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigaddset(&set, SIGUSR1);
	check(sigprocmask(SIG_SETMASK, &set, NULL),"sigprocmask failed",0) ;

	pthread_mutex_init(&logmutex, NULL);

	//start reporter thread
	check(pthread_create(&reporter, NULL, reporterloop , NULL),"Reporter failed to start, plese retry!",0); 


	// create a socket, set option so it can reuse the same address
	check((server_socket = socket (AF_INET , SOCK_STREAM , 0)),"Failed to create socket",0) ;
	

	// to reuse address
	check(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)),"Failed setting socket options!",0);

	// initialize the address struct
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons (SERVERPORT) ; //port before defined 

	// bind it with the structure intialized
	check( bind(server_socket, (SA*)&server_addr, sizeof(server_addr)),"Bind Failed!",0);

	system("ip addr show| grep inet");
	printf("\n>> Share the ip address of this device with your clients...\n");

	// if the port was not changed and bind succeded noitify that is the port is changeble 
	if(SERVERPORT==4444){
		printf(">> Server on default port %i, change port with \"-p\"...", SERVERPORT);
		printf("\033[0;33m\n>> No need to specify port from client...\033[0m\n");
	}else{
		printf(">> Server on port %i\n", SERVERPORT);
	}

	// set the socket in listen mode, so can accept future connections
	check( listen(server_socket, SERVER_BACKLOG),"Listen Failed!",1);

	// make or find the directory for the server's data
	dir_check = mkdir("DATA", 0777);
	if (dir_check==-1 && errno != EEXIST){
		printf(">> Failed to initialize server dirctory!");
		pthread_exit(0);
	}


	// set curr working dir
	chdir("DATA");

	// create admin user directory and put in it standard files for users + log_report + blacklist
	// pws file is not created so the admin once logged can finish the compilation of th account 
	mkdir("admin",0777);

	sprintf(buf,"admin/log_report");
	check(fd = open(buf, O_RDWR | O_CREAT , 0666),"Failed to open psw file",0);
	close(fd);

	sprintf(buf,"admin/backup");
	check(fd = open(buf, O_RDWR | O_CREAT , 0666),"Failed to open psw file",0);
	close(fd);

	sprintf(buf,"admin/black_list");
	check(fd = open(buf, O_RDWR | O_CREAT , 0666),"Failed to open psw file",0);
	close(fd);

	buffer	= malloc(sizeof(char)*BUFSIZE);
	struct timeval timer;

	while (true) {
		printf (">> Waiting for connections... \n") ;

		p_client = malloc(sizeof(client));

		//wait for, and eventually accepting an incoming connection
		addr_size = sizeof(SA_IN);
		check(client_socket = accept(server_socket, (SA*)&(p_client-> addr),(socklen_t*)&addr_size),">> Accept failed!", 0);

		//set socket timeout
		timer.tv_sec = TIMER;
		timer.tv_usec = 0;
		check(setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer)),"Failed setting socket options!",0);

		//check if the ip address is banned 
		ptr = fopen(buf,"r+");

		while(fgets(buffer, BUFSIZE, ptr)){
			if (strstr(buffer,inet_ntoa(p_client-> addr.sin_addr))){
				banned = 1;
				break;
			}
		}

		if((int)MAXONLINE <= currsession->users){

			printf (">> Connection from %s declined... \n", inet_ntoa(p_client-> addr.sin_addr)) ;

			//IP is in the blacklist, deny connection
			sprintf(buffer, "Spiacente, il server ha raggiunto la capacità massima.\n\tRiprova tra poco.");
			send(client_socket, buffer, strlen(buffer), MSG_NOSIGNAL);

			close(client_socket);
			banned = 0;

		} else if (banned == 0 ){

			currsession->users++;

			printf (">> Connection from %s accepted in %i... \n", inet_ntoa(p_client-> addr.sin_addr), client_socket) ;

			p_client-> ptr = client_socket;
			if (pthread_create(&t, NULL, handle_connection, (void*)p_client)<0){

				//reached maxium number of threads for this process
				sprintf(buffer, "Spiacente, il server non accetta nuove connesioni al momento, ritenta più tardi...");
				send(client_socket, buffer, strlen(buffer), MSG_NOSIGNAL);

				close(client_socket);
				banned = 0;
			}
		} else {
			printf (">> Connection from %s declined... \n", inet_ntoa(p_client-> addr.sin_addr)) ;

			//IP is in the blacklist, deny connection
			sprintf(buffer, "Spiacente, le attivita da questo indirizzo sono state limitate.");
			send(client_socket, buffer, strlen(buffer), MSG_NOSIGNAL);

			close(client_socket);
			banned = 0;
		}
		fclose(ptr);
	}

	return 0;
}

void* handle_connection (void* p_client){

// This is a client's thread: once the main thread accepted a client it redirects the job to one of this threads. 
// As a thread it has all the __thred variables, meaning they dont have to be declared in this scope, check server.h. 


	// some variables
	DIR* 	dp, *dp1;
  	struct 	dirent *ep;
  	S_ACT 	action = { 0 };
  	REPORT* temp;

  	client *client1 = (client*) p_client;

	fd = client1->ptr;
	client_addr = client1->addr;

	
	buffer	= malloc(sizeof(char)*BUFSIZE);
	data 	= malloc(sizeof(REPORT));

//	data is a REPORT struct, meaning it's a node of the status list.
	data->references = client1;

  	time_t 	t = time(NULL);
  	struct 	tm tm = *localtime(&t);
  	
	//ask for user
	memset(buffer,0,BUFSIZE);
	sprintf(buffer, "Inserire un nome utente:");

ASK:
	ask(fd,&buffer);

	if (strlen(buffer)<1 || strlen(buffer)>16){
		sprintf(buffer, "Inserire un nome utente valido (min 1- max 16 carattere/i):");
		goto ASK;
	}

	user = strdup(buffer);
	printf(">> Login attempt <%s>...\n",user);

	if(maintenance){	//maintenance check
		if (strcmp("admin",user)!=0){
				printf (">> Connection from %s declined... \n", inet_ntoa(client_addr.sin_addr));
				sprintf(buffer, "Spiacente, il server è in manutenzione.");
				send(fd, buffer, strlen(buffer), MSG_NOSIGNAL);
				close(fd);
		}
	}
	
	//serch for a directory for this user
	dp = opendir(getcwd(dir,256)); 
  	if (dp != NULL){
  		while((ep = readdir(dp)) != NULL){
  			count +=1;
    		if (strcmp(ep->d_name,user)==0){
    			//a directoy named after this user exists

    			// if the user registration isn't complete the directory doesnt contains psw file, trying to open
    			sprintf(dir,"%s/psw",user);

    			file = open(dir, O_RDWR , 0666);

    			if(file == -1){
    				// someone sent messages to this user before this registration

					sprintf(buffer,"L'utente è gia presente nel sistema, coninuare terminado la registrazione?(y/N)");
					ask(fd, &buffer);
					if (buffer[0]!='Y' && buffer[0]!='y'){
						sprintf(buffer, "Cancello iscrizione... Inserire nome utente:");
						goto ASK;
					}
					get_psw();
					closedir(dp);
					goto CASE0;	
   				}

   				//user directory contains psw file
   				sprintf(buffer,"Utente già registrato, inserire password per accedere:");
   				check(read(file, temp1, 16),"Failed to read password!",file);

				while(1){
					ask(fd, &buffer);


					if (!strcmp(temp1,buffer)){
						printf(">> User <%s> logged in...\n", user);
						close(file);


						sprintf(buffer,"Bentornato/a %s!", user);
						break;
					}else{
						sprintf(buffer,"Password errata! Ritenta o riconnetti al server per cambiare utente...");
					}
				}
				closedir(dp);
				memset(temp1,0,BUFSIZE);
				goto CASE0;
  			}
  		}
		//user not found, enrolling...
    	sprintf(buffer,"Utente non esistente, si desidera creare un nuovo utente con questo nome?(y/N)");
    	ask(fd, &buffer);

		if (buffer[0]=='Y'||buffer[0]=='y'){
			if (count>=MAXACCOUNTS){
				sprintf(buffer,"Spiacente, non si accettano nuove iscrizioni. Puoi accedere ad un account già esistente inserendo il nome utente:");
				goto ASK;
			}
			//make a new directory for the new user
			printf(">> Enrolling new user <%s>...\n",user);
			mkdir(user, 0777);

			get_psw();	
		}else{
			sprintf(buffer, "Cancello iscrizione... Inserire nome utente:");
			goto ASK;
		}
  	}else{
  		printf(">> Failed to scan through directorys! Terminating server!");
		exit(0);
  	}


CASE0:

// new user logged in, create a new node in the status list (which is a pointed list)
	memcpy(&data->user,user,strlen(user));	
	data->code = 1;
	append(head,data);
	pthread_kill(reporter, SIGUSR1);

//check for new messages and send them if any 
	check4new(1);

	while(1){

		memset(temp1,0,BUFSIZE);
		memset(temp2,0,BUFSIZE);
		memset(dir,0,256);
		memset(dir2,0,256);

		ask(fd, &buffer);
		ca = strdup(buffer);

		//client can ask any of this services

		if((ca[0]=='e'&& strlen(ca)==1)||(strcmp(ca,"exit")==0)){

			data->code = 2;
			currsession->users--;

			sprintf(buffer,"exit\n");
			write(fd, buffer, strlen(buffer));
			printf(">> <%s> logged out...\n",user);
			pthread_kill(reporter, SIGUSR1);
			break;
			
		}else if ((ca[0]=='l'&& strlen(ca)==1)||(strcmp(ca,"login")==0)){

			data->code = 2;
			currsession->users--;

			pthread_kill(reporter, SIGUSR1);

			pthread_t t;
			pthread_create(&t, NULL, handle_connection, (void*)p_client);
			pthread_exit(0);

		}else if ((ca[0]=='s'&& strlen(ca)==1)||(strcmp(ca,"send")==0)){
			sprintf(buffer,"Chi è il destinatario del messaggio?");

			do{
				ask(fd, &buffer);
				if(strlen(buffer)>0 && strlen(buffer)<17){
					dest = strdup(buffer);
					break;
				}else{
					memset(buffer,0,BUFSIZE);
					sprintf(buffer,"Inserire un destinatario valido...");
				}
			}while(1);

NEWUSER:
			dp1 = opendir(getcwd(dir,256));
			if (dp1 != NULL){
				bool c=false;
  				while((ep=readdir(dp1))!= NULL){	
    				if (strcmp(ep->d_name,dest)==0){
						c = true;						
						sprintf(dir,"%s/new",dest);
						sprintf(dir2,"%s/backup",dest);

                        check(file = open(dir, O_RDWR | O_CREAT | O_APPEND, 0666),"Failed to send message!",0);
						check(file2 = open(dir2, O_RDWR | O_CREAT | O_APPEND, 0666),"Failed to send message!",0);

						sprintf(buffer,"\033[0;32m%s\033[0m >> Qual'è l'oggetto del messaggio?",dest);
						ask(fd,&buffer);

						if(strlen(buffer)==0){
							sprintf(buffer,"/no object/ ");
						}else{
							strcpy(temp2,buffer);
							sprintf(buffer,"\"%s\"", temp2);
						}

						sprintf(temp1,"\t%02d:%02d %d-%02d-%02d da %s >> %s: ", tm.tm_hour, tm.tm_min, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, user,buffer);
						check(write(file, temp1, strlen(temp1)), "Failed to write date on message!",file);
						check(write(file2, temp1, strlen(temp1)), "Failed to write date on message!",file2);
						memset(temp1,0,BUFSIZE);

						sprintf(buffer,"\033[0;32m%s\033[0m >> Inserisci il messaggio...",dest);
						ask(fd, &buffer);

						if(strlen(buffer)==0){
							sprintf(buffer,"/no text/\n");
						}else{
							strcpy(temp2,buffer);
							sprintf(buffer,"%s\n", temp2);
						}

						check(write(file, buffer, strlen(buffer)), "Failed to write message!",file);
						check(write(file2, buffer, strlen(buffer)), "Failed to write message!",file2);

						printf(">> Message from <%s> to <%s> sent...\n", user, dest);
						close(file);
						close(file2);
						sprintf(buffer,"Messaggio inviato. Cos'altro fare?");
					}
				}
				if (!c){
					sprintf(buffer,"Utente non trovato. Inviare lo stesso?(y/N)\n\t\033[0;33mIl destinatario potrà leggere i messaggi una volta iscritto.\033[0m");
					ask(fd,&buffer);
					if ((buffer[0]=='Y'||buffer[0]=='y')){

						mkdir(dest,0777);
						goto NEWUSER;
					}else{
						sprintf(buffer,"Cos'altro fare?");
					}
				}
			}else{
				printf(">> Failed to scan through directorys! Terminating server!");
				exit(0);
			}
		}else if((ca[0]=='b'&& strlen(ca)==1)||(strcmp(ca,"backup")==0)){
				
				memset(dir,0,256);
				sprintf(dir,"%s/backup",user);
				check(file = open(dir, O_RDWR | O_CREAT , 0666),"Failed to open backup!",0);
				
				
				check(size = read(file, temp1, BUFSIZE), "Failed to upload backup!",file);
				if (size>0){


					// ready for stream message
					memset(temp2,0,BUFSIZE);
					sprintf(temp2,"/*");
					check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",file);

					sprintf(temp2,"Ecco il backup di questo account:\n\n");
					check(write(fd, temp2 , BUFSIZE), "Failer to upload backup!",file);


					do{
						check(write(fd, temp1, BUFSIZE), "Failed to upload backup!",file);
						memset(temp1,0,BUFSIZE);
						check(size = read(file, temp1, BUFSIZE), "Failed to upload backup!",file);

						if(size <= 0){
							sprintf(buffer,"\n\n\tCos'altro fare?\n*/");
							break;
						}

					}while(1);

					printf(">> Uploaded <%s>'s backup...\n", user);	
					
				}else{
					sprintf(buffer,"Nessun messaggio presente nel backup.\n\tCos'altro fare?");
				}
				close(file);
		}else if((ca[0]=='d'&& strlen(ca)==1)||(strcmp(ca,"delete")==0)){
			
			int n = 1;
			char count[BUFSIZE+10];

			memset(dir,0,256);
			sprintf(dir,"%s/backup",user);

			ptr = fopen(dir, "r+");
			
			
			fgets(temp1, BUFSIZE-30, ptr);

			if (strlen(temp1)>0){

				// ready for stream message
				memset(temp2,0,BUFSIZE);
				sprintf(temp2,"/*");
				check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",file);

				sprintf(temp2,"Ecco il backup di questo account:\n");
				check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",file);

				do{	
					if(strlen(temp1)<(BUFSIZE-30)){
						sprintf(count,"   \033[0;32m%d\033[0m\b\b\b%s",n,temp1);
						strcpy(temp1,count);
						n++;
					}

					check(write(fd, temp1, BUFSIZE), "Failed to upload backup!",file);
					memset(temp1,0,BUFSIZE);
					fgets(temp1, BUFSIZE-30, ptr);

					if(strlen(temp1)==0){
						sprintf(buffer,"\033[0;33m\n\ta(all)    - digitare a/all per eliminare l'intero backup \n\tx z y... - digitare i numeri in verde corrispondenti ai messaggi da eliminare\n\n\t\033[0mPer annullare la modifica premere una qualsiasi lettera o invio.\n*/");							
						break;
					}
				}while(1);

				fclose(ptr);

				ask(fd, &buffer);

				if ((buffer[0]=='a'&& strlen(buffer)==1)||(strcmp(buffer,"all")==0)){

					sprintf(dir,"%s/backup",user);
					ptr = fopen(dir, "w+");
					fclose(ptr);

					printf(">> <%s>'s backup  has been erased...\n", user);
					sprintf(buffer,"Backup elimanto con successo! Cos'altro fare?");

				}else if(strlen(buffer)>0){
					int 	*lines, i = 0;	
					char	*token = strtok(buffer, " ");

	   				lines = malloc(sizeof(int)*256);

	   				while( token != NULL ) {   				
	    				if(strlen(token)>0){
	    					lines[i] = (int)strtol(token, NULL, 0);
	    					i++;
	    				}
	    				token = strtok(NULL, " ");
	    			}

					sprintf(dir,"%s/backup",user);
					file = open(dir, O_RDWR ,0666);

					sprintf(dir2,"%s/backup_2",user);
					file2 = open(dir2, O_CREAT | O_RDWR | O_TRUNC ,0666);

					size = read (file, temp1, BUFSIZE);
					i = 0;

					while(size>0){
						token = strtok(temp1, "\n");
		    			while( token != NULL ) {
		    				i++;

		    				if(linetodelete(i,lines) == 0){
		    					sprintf(temp2,"%s\n",token);
		    					write(file2, temp2, strlen(temp2));
		    				}
		    				token = strtok(NULL, "\n");
		    			}
		    			size = read (file, temp1, BUFSIZE);
					}

					close(file);
					close(file2);

					check(remove(dir),"Failed to remove old backup...",fd);
					check(rename(dir2,dir),"Failed to replace old backup...",fd);
					printf(">> Deleted some messages from <%s>'s backup...\n",user);	    			
					sprintf(buffer,"La modifica è stata applicata. Cos'altro fare?");
				}else{
					sprintf(buffer,"Nessun messaggio eliminato.\n\tCos'altro fare?");
				}
			}else{
				sprintf(buffer,"Nessun messaggio presente nel backup.\n\tCos'altro fare?");
			}
		}else if((ca[0]=='n'&& strlen(ca)==1)||(strcmp(ca,"new")==0)){
			sprintf(buffer,"Controllo nuovi messaggi...");
			check4new(0);
		}else if((strcmp(ca,"ar")==0)||(strcmp(ca,"report")==0)){

				memset(temp1,0,BUFSIZE);
				memset(buffer,0,BUFSIZE);

				sprintf(dir,"%s/log_report",user);
				file = open(dir, O_RDWR , 0666);

				if(file == -1 && strcmp("admin",user)!=0){
					sprintf(buffer,"Autorizzazione negata, solo l'account \"admin\" puo accedere a questa funzionalità.\nCos'altro fare?");
				}else{
					
					check(size = read(file, temp1, BUFSIZE), "Failed to upload backup!",file);
					if (size>0){

						// ready for stream message
						memset(temp2,0,BUFSIZE);
						sprintf(temp2,"/*");
						check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",fd);

						sprintf(temp2,"\033[0;32mADMIN\033[0m -- Ecco i login/logout di tutti gli utenti iscritti:\n");
						check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",file);


						do{

							check(write(fd, temp1, strlen(temp1)), "Failed to upload backup!",file);
							memset(temp1,0,BUFSIZE);
							check(size = read(file, temp1, BUFSIZE), "Failed to upload backup!",file);
							if(size <= 0){
								sprintf(buffer,"\n\tCos'altro fare?\n*/");
								break;
							}
						}while(1);
						memset(temp1,0,BUFSIZE);

						printf(">> Uploaded log_report...\n");
						
					}else{
						sprintf(buffer,"\033[0;32mADMIN\033[0m -- Nessun report disponibile.\n\tCos'altro fare?");
					}
				close(file);
				}
		}else if((strcmp(ca,"abl")==0) || (strcmp(ca,"show bl")==0)){
			if(ptr == -1 || strcmp("admin",user)!=0){
				sprintf(buffer,"Autorizzazione negata, solo l'account \"admin\" puo accedere a questa funzionalità.\nCos'altro fare?");
			}else{
				memset(dir,0,256);
				sprintf(dir,"%s/black_list",user);
				check(file = open(dir, O_RDWR | O_CREAT , 0666),"Failed to open blacklist!",0);
				printf(">> Uploading blacklist...");
				
				check(size = read(file, temp1, BUFSIZE), "Failed to upload blacklist!",file);
				if (size>0){


					// ready for stream message
					memset(temp2,0,BUFSIZE);
					sprintf(temp2,"/*");
					check(write(fd, temp2 , BUFSIZE), "Failed to upload blacklist!",file);

					sprintf(temp2,"\033[0;32mADMIN\033[0m -- Ecco la blacklist di questo server:\n");
					check(write(fd, temp2 , BUFSIZE), "Failer to upload blacklist!",file);


					do{
						check(write(fd, temp1, BUFSIZE), "Failed to upload blacklist!",file);
						memset(temp1,0,BUFSIZE);
						check(size = read(file, temp1, BUFSIZE), "Failed to upload blacklist!",file);

						if(size <= 0){
							sprintf(buffer,"\n\tCos'altro fare?\n*/");
							break;
						}

					}while(1);

					printf(">> Uploaded <%s>'s blacklist...\n", user);	
					
				}else{
					sprintf(buffer,"\033[0;32mADMIN\033[0m -- Nessun IP presente nella blacklist.\n\tCos'altro fare?");
				}
				close(file);
			}
		}else if((strcmp(ca,"anb")==0)||(strcmp(ca,"new bl")==0)){
			if(ptr == -1 || strcmp("admin",user)!=0){
				sprintf(buffer,"Autorizzazione negata, solo l'account \"admin\" puo accedere a questa funzionalità.\nCos'altro fare?");
			}else{
				sprintf(dir,"%s/black_list",user);

	            check(file = open(dir, O_RDWR | O_CREAT | O_APPEND, 0666),"Failed to open blacklist!",0);

				sprintf(buffer,"\033[0;32mADMIN\033[0m -- Inserisci un indirizzo IP da cui rifiutare le connesioni...");
				ask(fd, &buffer);

				if(strlen(buffer)!=0){
					strcpy(temp2,buffer);
					sprintf(buffer,"\t%s\n", temp2);

					check(write(file, buffer, strlen(buffer)), "Failed to write message!",file);

					printf(">>Connnections from %s will no longer be accepted...\n", buffer);
					close(file);
					sprintf(buffer,"\033[0;32mADMIN\033[0m -- IP correttamente aggiunto alla blacklist. Cos'altro fare?");
				}else{
					sprintf(buffer,"\033[0;32mADMIN\033[0m -- Nessun IP è stato aggiunto alla blacklist. Cos'altro fare?");
				}
			}
			printf(">> New entry in the blacklist...");
		}else if((strcmp(ca,"acb")==0)||(strcmp(ca,"change bl")==0)){
			
			int n = 1;
			char count[BUFSIZE+10];

			memset(dir,0,256);
			sprintf(dir,"%s/black_list",user);

			ptr = fopen(dir, "r+");
			
			
			if(ptr == -1 || strcmp("admin",user)!=0){
				sprintf(buffer,"Autorizzazione negata, solo l'account \"admin\" puo accedere a questa funzionalità.\nCos'altro fare?");
			}else{

				fgets(temp1, BUFSIZE-30, ptr);
				if (strlen(temp1)>0){
					// ready for stream message
					memset(temp2,0,BUFSIZE);
					sprintf(temp2,"/*");
					check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",file);

					sprintf(temp2,"\033[0;32mADMIN\033[0m -- Ecco la blacklist di questo server:\n");
					check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",file);

					do{	
						if(strlen(temp1)<(BUFSIZE-30)){
							sprintf(count,"   \033[0;32m%d\033[0m\b\b\b%s",n,temp1);
							strcpy(temp1,count);
							n++;
						}

						check(write(fd, temp1, BUFSIZE), "Failed to upload backup!",file);
						memset(temp1,0,BUFSIZE);
						fgets(temp1, BUFSIZE-30, ptr);

						if(strlen(temp1)==0){
							sprintf(buffer,"\033[0;33m\n\ta(all)    - digitare a/all per eliminare l'intero blacklist \n\tx z y... - digitare i numeri in verde corrispondenti agli IP da eliminare\n\n\t\033[0mPer annullare la modifica premere una qualsiasi lettera o invio.\n*/");							
							break;
						}
					}while(1);

					fclose(ptr);

					ask(fd, &buffer);

					if ((buffer[0]=='a'&& strlen(buffer)==1)||(strcmp(buffer,"all")==0)){

						sprintf(dir,"%s/black_list",user);
						ptr = fopen(dir, "w+");
						fclose(ptr);

						printf(">> <%s>'s blacklist has been erased...\n", user);
						sprintf(buffer,"\033[0;32mADMIN\033[0m -- Blacklist svuotata con successo! Cos'altro fare?");

					}else if(strlen(buffer)>0){
						int 	*lines, i = 0;	
						char	*token = strtok(buffer, " ");

		   				lines = malloc(sizeof(int)*256);

		   				while( token != NULL ) {   				
		    				if(strlen(token)>0){
		    					lines[i] = (int)strtol(token, NULL, 0);
		    					i++;
		    				}
		    				token = strtok(NULL, " ");
		    			}

						sprintf(dir,"%s/black_list",user);
						file = open(dir, O_RDWR ,0666);

						sprintf(dir2,"%s/black_list_2",user);
						file2 = open(dir2, O_CREAT | O_RDWR | O_TRUNC ,0666);

						size = read (file, temp1, BUFSIZE);
						i = 0;

						while(size>0){
							token = strtok(temp1, "\n");
			    			while( token != NULL ) {
			    				i++;

			    				if(linetodelete(i,lines) == 0){
			    					sprintf(temp2,"%s\n",token);
			    					write(file2, temp2, strlen(temp2));
			    				}
			    				token = strtok(NULL, "\n");
			    			}
			    			size = read (file, temp1, BUFSIZE);
						}

						close(file);
						close(file2);

						check(remove(dir),"Failed to remove old blacklist...",fd);
						check(rename(dir2,dir),"Failed to replace old balcklist...",fd);
						printf(">> Deleted some IPs from the blacklist...\n",user);	    			
						sprintf(buffer,"\033[0;32mADMIN\033[0m -- La modifica è stata applicata. Cos'altro fare?");
					}else{
						sprintf(buffer,"\033[0;32mADMIN\033[0m -- Nessun IP eliminato.\n\tCos'altro fare?");
					}
				}else{
					sprintf(buffer,"\033[0;32mADMIN\033[0m -- Nessun IP presente nella blacklist.\n\tCos'altro fare?");
				}
				printf(">> Blacklist updated...");
			}
		}else if((strcmp(ca,"am")==0)||(strcmp(ca,"maintenance")==0)){
			if(ptr == -1 || strcmp("admin",user)!=0){
				sprintf(buffer,"Autorizzazione negata, solo l'account \"admin\" puo accedere a questa funzionalità.\nCos'altro fare?");
			}else{
				maintenance = !maintenance;
				if(maintenance){

					printf(">> Maintenance ON...");
					temp = head->next;

					pthread_mutex_lock(&logmutex);

		    		while(temp != NULL){
		    			temp->code == 3;
		    			if (temp->references->ptr != fd){
		    				close(temp->references->ptr);
		    			};
		    			temp = temp->next;
		    		}
		    		pthread_mutex_unlock(&logmutex);

		    		pthread_kill(reporter, SIGUSR1);

					sprintf(buffer,"\033[0;32mADMIN\033[0m -- Modalità manutenzione attivata.\n\tCos'altro fare?");
				}else{
					sprintf(buffer,"\033[0;32mADMIN\033[0m -- Modalità manutenzione disattivata.\n\tCos'altro fare?");
					printf(">> Maintenance OFF...");
				}
			}
		}else if((strcmp(ca,"au")==0)||(strcmp(ca,"users")==0)){
			if(ptr == -1 || strcmp("admin",user)!=0){
				sprintf(buffer,"Autorizzazione negata, solo l'account \"admin\" puo accedere a questa funzionalità.\nCos'altro fare?");
			}else{
				sprintf(buffer,"/*");
				check(write(fd, buffer, BUFSIZE), "Failed to upload list!",file);

				sprintf(buffer,"\033[0;32mADMIN\033[0m -- Ecco gli utenti online in questo momento:\n\n");
				check(write(fd, buffer, BUFSIZE), "Failed to upload list!",file);

				temp = head->next;
				count = 0;

		    	while(temp != NULL){
		    		count ++;

		    		sprintf(buffer,"  \033[0;32m%d\033[0m\b\b\b\t%s -> %s\n",count,temp->user, inet_ntoa(temp->references->addr.sin_addr));
		    		check(write(fd, buffer, BUFSIZE), "Failed to upload list!",0);

		    		temp = temp->next;
		    	}
		    	sprintf(buffer,"\n\tCos'altro fare?\n*/");
			}
		}else if((ca[0]=='h'&& strlen(ca)==1)||(strcmp(ca,"help")==0)){			

			sprintf(dir,"../help",user);
			ptr = fopen(dir, "r+");
			
			if (ptr!=NULL){
				fgets(temp1, BUFSIZE, ptr);
				if (strlen(temp1)>0){

					// ready for stream message
					memset(temp2,0,BUFSIZE);
					sprintf(temp2,"/*");
					check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",0);

					sprintf(temp2,"Questo puo essere d'aiuto:\n");
					check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",0);

					sprintf(temp2,"\033[0;33m\n");
					check(write(fd, temp2 , BUFSIZE), "Failed to upload backup!",0);

					do{	

						check(write(fd, temp1, BUFSIZE), "Failed to upload backup!",0);
						memset(temp1,0,BUFSIZE);
						fgets(temp1, BUFSIZE, ptr);

					}while(strlen(temp1)!=0);

					fclose(ptr);
					sprintf(buffer,"\n\033[0m\tCosa fare?\n*/");
					
				}
			}else{

				sprintf(buffer,"Contattare l'utente admin per una segnalazione.\n (Help file could not be found)...");
			}
		}else{
			memset(buffer,0,BUFSIZE);
			sprintf(buffer,"Comando (\"%s\") non riconosciuto\n\tPer info sui comandi digitare \"h\" o \"help\"...",ca);
		}

	}
		
	close(fd);	
	pthread_exit(0);
	return NULL;
}


void* reporterloop(){

	// Special thread: updates the log_report file in DATA/admin. Any new entry will be set from status code 1 to 0,
	// meaning that user is online. Once the code changes from 0 to 2 or -1 the entry in the list will be popped meaning the user is
	// no longer online. The list will be checked every time a signal SIGUSR1 arrives. This means that a client?s thread added/changed
	// something.


	REPORT* temp;
	time_t 	t = time(NULL);
  	struct 	tm tm = *localtime(&t);
	buffer	= malloc(sizeof(char)*BUFSIZE);
	sigset_t oldset;

	action.sa_sigaction = &sigusr1_handler;
	sigaction(SIGUSR1,&action,NULL);

	while(1){
		sigpending(&set);

		if (sigismember(&set, SIGUSR1)){
			sigemptyset(&set);
			if (sigprocmask(SIG_SETMASK, &set, NULL) != 0)
					perror("sigprocmask() error");	

			temp = head->next;

		    while(temp != NULL){

				if(temp->code != 0){
				  	sprintf(buffer,"admin/log_report");
					check(file = open(buffer, O_RDWR| O_CREAT | O_APPEND, 0666),"Failed to open adm/login file",0);
					if (temp->code == 1){
						sprintf(buffer,"\t%02d:%02d %d-%02d-%02d user %s from %s: LOGGED IN\n", tm.tm_hour, tm.tm_min, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, temp->user, inet_ntoa(temp->references->addr.sin_addr));
						temp->code = 0;
						printf("\t  --> %s -- Login registered\n", temp->user);
						temp = temp->next;
						
					}
					else if (temp->code == 2){
						sprintf(buffer,"\t%02d:%02d %d-%02d-%02d user %s from %s: LOGGED OUT \n", tm.tm_hour, tm.tm_min, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, temp->user ,inet_ntoa(temp->references->addr.sin_addr));
						data = temp;
						printf("\t  --> %s -- Logout registered\n", temp->user);
						temp = temp->next;
						pop(data);
						
					}			
					else if (temp->code == -1){
						sprintf(buffer,"\t%02d:%02d %d-%02d-%02d user %s from %s: LOST CONNECTION \n", tm.tm_hour, tm.tm_min, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, temp->user ,inet_ntoa(temp->references->addr.sin_addr));
						data = temp;
						printf("\t  --> %s -- Crash registered\n", temp->user);
						temp = temp->next;
						pop(data);
					}
					else if (temp->code == 3){
						sprintf(buffer,"\t%02d:%02d %d-%02d-%02d user %s from %s: FORCED LOGGGED OUT \n", tm.tm_hour, tm.tm_min, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, temp->user ,inet_ntoa(temp->references->addr.sin_addr));
						data = temp;
						printf("\t  --> %s -- Forced logout\n", temp->user);
						temp = temp->next;
						pop(data);
					}


					check(write(file,buffer,strlen(buffer)),"Failed to write log_report",file);
					close(file);
				}else{
					temp = temp->next;
				}
			}
			sigaddset(&set, SIGUSR1);
			if (sigprocmask(SIG_SETMASK, &set, NULL) != 0)
      			perror("sigprocmask() error");
		}
		sigemptyset(&set);
	}
}
void sigusr1_handler(int dummy, siginfo_t* dummy1 , void* dummy2){
	printf("-- Signal --> Checking for status updates....\n");
}

void sigpipe_handler(int dummy, siginfo_t* dummy1 , void* dummy2){
	printf(">> Unexpected disconnection occured, closing connection...\n");
	//this should not happen by the way but if it happens here it is 
}
