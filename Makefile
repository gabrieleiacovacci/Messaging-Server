#Gabriele Iacovacci 0299026 
#Progetto Sistemi Operativi 2022/2023

SERVER	= server
CLIENT	= client

SSRC	= server42.c
CSRC	= client.c

all: $(CLIENT) $(SERVER) 
 
$(SERVER): $(SSRC) 
	gcc $(SSRC) -lpthread -w -o $(SERVER)
	@printf " >> %s\n" "server executable has been compiled successfully..."
	
$(CLIENT): $(CSRC) 
	gcc $(CSRC) -o $(CLIENT)
	@printf " >> %s\n" "client executable has been compiled successfully..."


clean:
	@rm -f $(CLIENT)
	@rm -f $(SERVER)
	@printf " >> %s\n" "cleaning directory..."

erase: 
	@rm -f $(CLIENT)
	@rm -f $(SERVER)
	@sudo rm -rf DATA
	@printf " >> %s\n" "server's data has been deleted..."
