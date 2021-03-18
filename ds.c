
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
struct sockaddr_in peer_addr[MAX_PEER];   //array indirizzi registrati
int num_peer = 0;   //numero peer registrati

char buffer[BUFFER_LEN];

time_t rawtime;

char DS_file[] = "DS_register.txt";

struct Peer peer_registered[MAX_PEER];
char left_ip[ADDR_LEN];
char left_port[PORT_LEN];
char right_ip[ADDR_LEN];
char right_port[PORT_LEN];

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

	//inizializzo l'array di struct
	for(i = 0; i < MAX_PEER; i++)
		peer_registered[i].significant = 0;

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

			if(strcmp(command, "BOOT") == 0) {
				
				//controllo sul numero di peer registrati
				if(num_peer == MAX_PEER) {
					perror("Errore: raggiunto il numero massimo di peer\n");
					closing_actions();
					exit(-1);
				}
				//registrazione del nuovo peer
				for(i = 0; i < MAX_PEER; i++) {
					if ((atoi(second_arg) < atoi(peer_registered[i].port)) || (peer_registered[i].significant == 0))
						break;
				}  		//i è l'indice in cui inserire il nuovo peer
printf("*** I = %i\n", i);

				//se con significant == 1 faccio shift
		//					\/ I=2
		// 		|  x  |  x  |  x  |  x  |     |

				if(peer_registered[i].significant == 1) {
					for(j = num_peer-1; j >= i; j--) {
						peer_registered[j+1] = peer_registered[j];
						peer_addr[j+1] = peer_addr[j];
					}
				}	

				//altrimenti aggiungo di seguito
				peer_addr[i] = connecting_addr;

				strcpy(peer_registered[i].ip, first_arg);
				strcpy(peer_registered[i].port, second_arg);
				peer_registered[i].significant = 1;

				num_peer++;

printf("NUM_PEER: %i\n", num_peer);
				
				//invio le informazioni sui vicini

				if(num_peer <= 2) {

					if(num_peer == 1) {
						sprintf(buffer, "NEIGHBORS");
					}

					if (num_peer == 2) {
						strcpy(right_ip, peer_registered[1].ip);
						strcpy(right_port, peer_registered[1].port);
						sprintf(buffer, "NEIGHBORS %s %s", right_ip, right_port);

						printf("Aggiorno vicini di %s: %s\n", peer_registered[0].port, buffer);
						
						len = strlen(buffer)+1;

						ret = sendto(sd, buffer, len, 0,
							(struct sockaddr*)&peer_addr[0], sizeof(peer_addr[0]));
						if (ret < 0)
							perror("Errore invio risposta al peer\n");
					}

					strcpy(left_ip, peer_registered[(i-1)%MAX_PEER].ip);
					strcpy(left_port, peer_registered[(i-1)%MAX_PEER].port);
					strcpy(right_ip, peer_registered[(i+1)%MAX_PEER].ip);
					strcpy(right_port, peer_registered[(i+1)%MAX_PEER].port);
					sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);

					printf("Invio vicini: %s\n", buffer);

					len = strlen(buffer)+1;

					ret = sendto(sd, buffer, len, 0,
						(struct sockaddr*)&connecting_addr, sizeof(connecting_addr));
					if (ret < 0)
						perror("Errore invio risposta al peer\n");
					
				} else {

					if(num_peer > 2) {

						for(j = 0; j < num_peer; j++) {

							if((j == ((i+num_peer-1)%num_peer)) || (j == ((i+1)%num_peer))) {
								
								strcpy(left_ip, peer_registered[(j+num_peer-1)%num_peer].ip);
								strcpy(left_port, peer_registered[(j+num_peer-1)%num_peer].port);
								strcpy(right_ip, peer_registered[(j+1)%num_peer].ip);
								strcpy(right_port, peer_registered[(j+1)%num_peer].port);
								sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);
							
								printf("Aggiorno i vicini di %s: %s\n", peer_registered[j].port, buffer);

								len = strlen(buffer)+1;

								ret = sendto(sd, buffer, len, 0,
									(struct sockaddr*)&peer_addr[j], sizeof(peer_addr[j]));
								if (ret < 0)
									perror("Errore invio risposta al peer\n");
							}
						}

						strcpy(left_ip, peer_registered[(i-1)%num_peer].ip);
						strcpy(left_port, peer_registered[(i-1)%num_peer].port);
						strcpy(right_ip, peer_registered[(i+1)%num_peer].ip);
						strcpy(right_port, peer_registered[(i+1)%num_peer].port);
						sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);

						printf("Invio vicini: %s\n", buffer);

						len = strlen(buffer)+1;

						ret = sendto(sd, buffer, len, 0,
							(struct sockaddr*)&connecting_addr, sizeof(connecting_addr));
						if (ret < 0)
							perror("Errore invio risposta al peer\n");

					}
				}

/*
for(i = 0; i < 5; i++) {
	printf("IP: %s\n", peer_registered[i].ip);
	printf("PORT: %s\n", peer_registered[i].port);
	printf("SIGN: %i\n", peer_registered[i].significant);
}
*/

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
				for(i = 0; i < MAX_PEER; i++) {
					if(peer_registered[i].significant == 1) {
						ret = sendto(sd, buffer, len, 0,
							     (struct sockaddr*)&peer_addr[i], sizeof(peer_addr[i]));
						if(ret < 0)
							sleep(POLLING_TIME);
					}
				}

				closing_actions();

				exit(0);
			}			
		}
	}

	closing_actions();
    
}

