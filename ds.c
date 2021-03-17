
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
#include "shared.h"

#define MAX_PEER        10
#define POLLING_TIME    5       //controllo ogni 5 secondi

int i, j, k;
int ret, sd, len, addrlen;

char peer_ip[ADDR_LEN];
char peer_port[PORT_LEN];
char ds_port[PORT_LEN];
char* tmp_port;

char* token;
char command[CMD_LEN];
char first_arg[BUFFER_LEN];
char second_arg[BUFFER_LEN];

fd_set master;		//set di tutti i descrittori
fd_set read_fds;	//set dei descrittori in lettura
int fdmax;

struct sockaddr_in my_addr, connecting_addr;    //struct per indirizzi
struct sockaddr_in peer_registered[MAX_PEER];   //array indirizzi registrati
int num_peer = 0;   //numero peer registrati

char buffer[BUFFER_LEN];

time_t rawtime;

char DS_file[] = "DS_register.txt";

//struct Boot peer_boot;
//struct Peer peer;

void closing_actions() {
	
	close(sd);
	FD_CLR(sd, &master);
	
	close(0);
	FD_CLR(0, &master);

	free(tmp_port);
	free(token);

}

void ds_connect() {

	//creazione socket UPD non bloccante
    sd = socket(AF_INET, SOCK_DGRAM, 0);

    //creazione indirizzo di bind
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4242);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    
    //aggancio
    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        perror("Bind non riuscita\n");
        exit(-1);
    }

	fdmax = sd;
	printf("Socket creato: DS in ascolto...\n");
//	printf("FDMAX: %i\n", fdmax);
//	printf("SD: %i\n", sd);

}

void parse_string(char buffer[]) {

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
			default:
				printf("Comando non riconosciuto\n");
				break;
		}
		
		token = strtok(NULL, " ");
	} 
}

int main(int argc, char* argv[]) {

	tmp_port = (char*)malloc(sizeof(char)*ADDR_LEN);
	if(tmp_port == NULL) {
		perror("Memory not allocated: \n");
		exit(0);
	}

	token = (char*)malloc(sizeof(char)*BUFFER_LEN);
	if(token == NULL) {
		perror("Memory not allocated: \n");
		exit(-1);
	}

	// Estraggo il numero di porta
	if(argc == 1) {
		perror("Comando non riconosciuto: inserire numero di porta\n");
		exit(-1);
	}
	if(argc > 2) {
		printf("Comando non riconosciuto\n");
		exit(-1);
	}
	if(argc == 2){
		strcpy(tmp_port, argv[argc-1]);
		strcpy(ds_port, tmp_port);		
//printf("NUMERO DI PORTA: %s\n", ds_port);
	}
/*
//reset dei descrittori
FD_ZERO(&master);			//svuota master
FD_ZERO(&read_fds);			//svuota read_fds

FD_SET(0, &master);			//aggiunge stdin a master
FD_SET(sd, &master);		//aggiunge sd a master*/

    printf("******************* DS COVID STARTED *******************\n");
	ds_connect();
	printf("Digita un comando:\n");  

    while(1) {

		FD_ZERO(&master);			//svuota master
		FD_ZERO(&read_fds);			//svuota read_fds

		FD_SET(0, &master);			//aggiunge stdin a master
		FD_SET(sd, &master);		//aggiunge sd a master

		read_fds = master;  
		select(fdmax+1, &read_fds, NULL, NULL, NULL);	
		// select ritorna quando un descrittore è pronto

		if (FD_ISSET(sd, &read_fds)) {  //sd pronto in lettura

			do {
				addrlen = sizeof(connecting_addr);
				ret = recvfrom(sd, buffer, BUFFER_LEN, 0,
						(struct sockaddr*)&connecting_addr, &addrlen);
				if(ret < 0) {
					perror("Errore richiesta dal peer\n");
					exit(-1);
				}
				printf("Messaggio ricevuto: %s\n", buffer);
			} while (ret < 0);

			parse_string(buffer);
/*
printf("COMMAND: %s\n", command);
printf("PEER_IP: %s\n", first_arg);
printf("PEER_PORT: %s\n", second_arg);
*/
			if(strcmp(command, "BOOT") == 0) {

				if(num_peer == MAX_PEER) {
					perror("Errore: raggiunto il numero massimo di peer\n");
					closing_actions();
					exit(-1);
				}
				//registrazione del nuovo peer
				num_peer++;
				peer_registered[num_peer] = connecting_addr;	
printf("NUM_PEER: %i\n", num_peer);

				printf("Invio vicini: %s\n", buffer);

				time(&rawtime);
				
				sprintf(buffer, "%s", ctime(&rawtime));
				len = strlen(buffer)+1;
				ret = sendto(sd, buffer, len, 0,
				     (struct sockaddr*)&connecting_addr, sizeof(connecting_addr));
				if (ret < 0)
					perror("Errore invio risposta al peer\n");
				//printf("FINE SEND TO\n");
			} 

		}

		if (FD_ISSET(0, &read_fds)) {  	//stdin pronto in lettura
			scanf("%[^\n]", buffer);
			scanf("%*c");
		
			parse_string(buffer);

			if(strcmp(command, "help") == 0) {  printf("DENTRO HELP\n");
				printf("\nDettaglio comandi:\n");
				printf("1) help 		--> mostra i dettagli dei comandi\n");
				printf("2) showpeers 		--> mostra un elenco dei peer connessi\n");
				printf("3) showneighbor <peer> 	--> mostra i neighbor di un peer\n");
				printf("4) esc 			--> chiude il DS\n\n");
			}

			if(strcmp(command, "showpeers") == 0) {
				
			}

			if(strcmp(command, "there's a problem'") == 0) {
				
			}

			if(strcmp(command, "esc") == 0) {

				sprintf(buffer, "%s", "ESC");
				len = strlen(buffer) + 1;
				ret = sendto(sd, buffer, len, 0,
				     (struct sockaddr*)&connecting_addr, sizeof(connecting_addr));

				closing_actions();

				exit(0);
			}			
		}
	}

	closing_actions();
    
}

