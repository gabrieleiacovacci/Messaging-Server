# Messaging Server
 
Gabriele Iacovacci 0299026 
Progetto Sistemi Operativi 2022/2023
Prof. Francesco Quaglia

Servizio di messaggistica:

	Realizzazione di un servizio di scambio messaggi supportato tramite un server
	sequenziale o concorrente (a scelta). Il servizio deve accettare messaggi 
	provenienti da client (ospitati in generale su macchine distinte da quella 
	dove risiede il server) ed archiviarli.  

	L'applicazione client deve fornire ad un utente le seguenti funzioni:
	-> 1. Lettura tutti i messaggi spediti all'utente.
	-> 2. Spedizione di un nuovo messaggio a uno qualunque degli utenti del sistema.
	-> 3. Cancellare dei messaggi ricevuti dall'utente.

	Un messaggio deve contenere almeno i campi Destinatario, Oggetto e Testo.
	Si precisa che la specifica prevede la realizzazione sia dell'applicazione client
	che di quella server. Inoltre, il servizio potra' essere utilizzato solo
	da utenti autorizzati (deve essere quindi previsto un meccanismo di autenticazione).                       
	
	Per progetti misti Unix/Windows e' a scelta quale delle due applicazioni 
	sviluppare per uno dei due sistemi.


Make shortcuts:

- make (default) 	compile server and client
- make server		compile server
- make client		compile client
- make clean		deletes executables
- make erase 		deletes all the server’s data and executables


Any User and client:

	->can access the server from the local network, knowing the right ip address and port.
	->both for the client and server the port is set to be 4444 if not specified, can be changed with -p flag.
	->can send, delete or see messages from other users or themself.
	->if a user does not exists yet, messages can be sent anyway. whenever that user will be enrolled those messages will be received.


User "admin":

	--before publishing the ip address of the server, make sure to own the admin account.
	->have access to all basic user services. like receiving messages, maybe about some bugs or questions.
	->owns a file with all the login/logout reports (log_report).
	->states a black list, connections from IP addresses contained in that list will be declined.(black_list).
	->there's no need to restart the server after updating the black list.
	->the maximum number of accounts is set to 100, just in case someone tries to overwhelm the system enrolling too many users. if so just put the IP address in the blacklist.


Reporter's Pointed List and Reporter thread:
	--a special thread serves as a reporter, to signal whether a user logged in/out or crashed.
	->every node of the pointed list works like a report: who, how is connected and what needs to be reported (using a code, check below).
	->the list is updated both from the clients's threads and the reporter but in different ways.
	->reporter checks the list whenever a signal SIGUSR1 is received. in fact no SIG-PIPE are intended to be received from this process at all, if happens there's an handler. 
	->not using an Hash table so more than one session per user can be provided.
	
	-------------------------------------------
	| code  | meaning			  |
	|-----------------------------------------|
	|   0   | online, do nothing...	 	  |
	|   1   | login, write than set to zero   |
	|   2   | logout, write than pop the node |
	|  -1   | crash, write than pop the node  |
	-------------------------------------------

