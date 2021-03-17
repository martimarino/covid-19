
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include "shared.h"

#define POLLING_TIME  5
#define RES_LEN       26    // Wed Dec 09 17:41:29 2020\0\n

int ret, sd, len, i;
char localhost[ADDR_LEN] = "127.0.0.1\0";
char peer_port[PORT_LEN];
char* tmp_port;
int peer_connected = 0;
char command[CMD_LEN+1];
char first_arg[BUFFER_LEN];
char second_arg[BUFFER_LEN];
char third_arg[BUFFER_LEN];

struct sockaddr_in srv_addr, my_addr;
char buffer[BUFFER_LEN];

// informazioni da inviare al server
char info[BUFFER_LEN];

//struct Request req;

fd_set master;		//set di tutti i descrittori
fd_set read_fds;	//set dei descrittori in lettura
int fdmax = 0;

char *token;

void peer_connect(char DS_addr[], char DS_port[]) {
    /* Creazione socket */
    sd = socket(AF_INET, SOCK_DGRAM, 0);

    /* Creazione indirizzo di bind */
    memset (&my_addr, 0, sizeof(my_addr));     // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(peer_port));
    my_addr.sin_addr.s_addr = INADDR_ANY;

    /* Aggancio */
    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret < 0){
        perror("Bind non riuscita\n");
        exit(0);
    }
    /* Creazione indirizzo del server */
    memset (&srv_addr, 0, sizeof(srv_addr)); 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(DS_port));
    inet_pton(AF_INET, DS_addr, &srv_addr.sin_addr);

	fdmax = sd;
	peer_connected = 1;
	printf("Connessione con il DS effettuata\n");
}

int main(int argc, char* argv[]){

	tmp_port = (char*)malloc(sizeof(char)*ADDR_LEN);
	if(tmp_port == NULL) {
		printf("Memory not allocated\n");
		exit(0);
	}

	token = (char*)malloc(sizeof(char)*BUFFER_LEN);
	if(token == NULL) {
		printf("Memory not allocated\n");
		exit(0);
	}

	// Estraggo il numero di porta
	if(argc == 1) {
		printf("Comando non riconosciuto: inserire numero di porta\n");
		exit(1);
	}
	if(argc > 2) {
		printf("Comando non riconosciuto\n");
		exit(1);
	}
	if(argc == 2){
		strcpy(tmp_port, argv[argc-1]);
		strcpy(peer_port, tmp_port);		
	}

/*
	//reset dei descrittori
	FD_ZERO(&master);			//svuota master
	FD_ZERO(&read_fds);			//svuota read_fds

	FD_SET(0, &master);			//aggiunge stdin a master
	FD_SET(sd, &master);		//aggiunge sd a master*/
	
	while(1) {

		//reset dei descrittori
		FD_ZERO(&master);			//svuota master
		FD_ZERO(&read_fds);			//svuota read_fds

		FD_SET(0, &master);			//aggiunge stdin a master
		FD_SET(sd, &master);		//aggiunge sd a master

		read_fds = master;  
		select(fdmax+1, &read_fds, NULL, NULL, NULL);	
		// select ritorna quando un descrittore è pronto

		if (FD_ISSET(sd, &read_fds) && (peer_connected == 1)) {  //sd pronto in lettura
			printf("Aspetto di ricevere...\n");
			//riceve comandi dal server
			do {
				/* Tento di ricevere i dati dal server  */
				ret = recvfrom(sd, buffer, BUFFER_LEN, 0, 
								(struct sockaddr*)&srv_addr, &len);

				/* Se non ricevo niente vado a dormire per un poco */
				if (ret < 0)
					sleep(POLLING_TIME);

			} while(ret < 0);
			printf("Ricevuto: %s\n", buffer);
			
		}

		if (FD_ISSET(0, &read_fds)) {  	//stdin pronto in lettura
			
			scanf("%[^\n]", buffer);
			scanf("%*c");
			
			token = strtok(buffer, " ");
		
			for(i = 0; token != NULL; i++) {
				
				switch(i) {
					case 0:
						sscanf(token, "%s", &command);
						break;
					case 1:
						sscanf(token, "%s", &first_arg);
						break;
					case 2:
						sscanf(token, "%s", &second_arg);
						break;
					case 3:
						sscanf(token, "%s", &third_arg);
						break;
					default:
						printf("Comando non riconosciuto\n");
						break;
				
				}
				token = strtok(NULL, " ");
			}

			if(strcmp(command, "start") == 0){
//printf("START COMMAND\n");
				printf("Richiesta connessione al DS...\n");
//printf("PEER_CONNECTED: %i\n", peer_connected);
				peer_connect(first_arg, second_arg);
//printf("FDMAX: %i\n", fdmax);
//printf("PEER_CONNECTED: %i\n", peer_connected);
				do {
					//copio la struct nel buffer da inviare
					sprintf(buffer, "BOOT %s %s", localhost, peer_port);

					printf("Boot message inviato: %s\n", buffer);
					// Tento di inviare le informazioni di boot continuamente        
					ret = sendto(sd, buffer, sizeof(buffer), 0,
									(struct sockaddr*)&srv_addr, sizeof(srv_addr));
					// Se la richiesta non e' stata inviata vado a dormire per un poco
					if (ret < 0)
								sleep(POLLING_TIME);
				} while (ret < 0);
			
				printf("INFO BOOT INVIATE\n");
				printf("Attesa risposta dal DS...\n");

			}
/*
			if(strcmp(command, "add") == 0) {	
				
			}

			if(strcmp(command, "get") == 0) {	

			}
*/	
			if(strcmp(command, "stop") == 0) {	
				printf("Terminazione forzata\n");
				free(tmp_port);
				free(token);
				if(peer_connected == 1) {
					close(sd);
					peer_connected = 0;
					printf("Chiusura del socket effettuata\n");
				}
				
				exit(0);
			}
		}	
	} //while

	free(tmp_port);
	free(token);
    printf("%s\n", buffer);

	//peer_connected = 0;
    close(sd);

} //main


