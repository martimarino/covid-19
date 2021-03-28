
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

#define MAX_PEER        5
#define POLLING_TIME    5   //controllo ogni 5 secondi

int i, j, k;
int ret, sd, len, addrlen;

char ds_port[PORT_LEN];
char* tmp_port;				//variabile per prelevare porta da terminale

//variabili per i comandi
int valid_input;			//indica se un comando ha formato corretto
char* token;				//serve per la strtok
int howmany;				//per comandi con campi opzionale
char command[CMD_LEN];	
char first_arg[BUFFER_LEN];
char second_arg[BUFFER_LEN];
char third_arg[BUFFER_LEN];

//variabili per la select
fd_set master;					//set di tutti i descrittori
fd_set read_fds;				//set dei descrittori in lettura
int fdmax;

struct sockaddr_in my_addr, connecting_addr;    //struct per indirizzi
struct sockaddr_in peer_addr[MAX_PEER];   		//array indirizzi registrati
int num_peer = 0;   							//numero peer registrati

char buffer[BUFFER_LEN];

struct Peer {
	char ip[ADDR_LEN];
	char port[PORT_LEN];
};
struct Peer peer_registered[MAX_PEER];
int already_reg = 0;

//variabili temporanee per la comunicazione dei vicini
char left_ip[ADDR_LEN];
char left_port[PORT_LEN];
char right_ip[ADDR_LEN];
char right_port[PORT_LEN];

struct Entry {
	char date[11];
	int num_peer_N;;
	int num_peer_T;
} DS_entry;

struct Entry findEntry;
time_t now;
struct tm *todayDateTime;
FILE* fd;
char filepath[BUFFER_LEN];
char DS_file[] = "DS_register.txt";
int counter = 0;

//variabili per GET
struct tm dateToConvert;
time_t min_date_given, max_date_given, date_tmp;

void closing_actions() {	//azioni da compiere quando DS termina
	
	close(sd);
	FD_CLR(sd, &master);
	
	close(0);
	FD_CLR(0, &master);

	free(tmp_port);
	free(token);

	fclose(fd);

}

void ds_connect() {		//creazione socket

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
}

void updateRegister() {
	time(&now);
	todayDateTime = localtime(&now);
	strftime(DS_entry.date, sizeof(DS_entry.date), "%d:%m:%Y", todayDateTime);
printf("STRUCT: %s, %i, %i\n", DS_entry.date, DS_entry.num_peer_N, DS_entry.num_peer_T);
	fprintf(fd, "%s %i %i\n", DS_entry.date, DS_entry.num_peer_N, DS_entry.num_peer_T);
}

int parse_string(char buffer[]) {	//separa gli argomenti di un comando

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

	//controlli formato messaggio
	if(((strcmp(command, "help") == 0) && (i != 1)) ||
		((strcmp(command, "showpeers") == 0) && (i != 1)) ||
		((strcmp(command, "showneighbor") == 0) && (i > 2)) ||
		((strcmp(command, "esc") == 0) && (i != 1))) {
		printf("Comando non riconosciuto\n");
		valid_input = 0;
	} else {
		valid_input = 1;
	}

	return i;
}

void sendToPeer(struct sockaddr_in addr) {
	len = strlen(buffer) + 1;
	do {
		ret = sendto(sd, buffer, len, 0,
				(struct sockaddr*)&addr, sizeof(addr));
		if(ret < 0)
			sleep(POLLING_TIME);
	} while (ret < 0);
}

void initDSfile() {
	//apertura file
	strcpy(filepath, "./");
	strcat(filepath, DS_file);
	fd = fopen(filepath, "a");
	if(fd == NULL)
		perror("Error: ");
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
	}

	initDSfile();

    printf("******************* DS COVID STARTED *******************\n");
	ds_connect();
	printf("Digita un comando:\n");  

	FD_ZERO(&master);			//svuota master
	FD_ZERO(&read_fds);			//svuota read_fds

	FD_SET(0, &master);			//aggiunge stdin a master
	FD_SET(sd, &master);		//aggiunge sd a master

    while(1) {

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

			howmany = parse_string(buffer);	

			if(strcmp(command, "BOOT") == 0) {

				already_reg = 0;
				for(i = 0; i < num_peer; i++) {
					if(strcmp(second_arg, peer_registered[i].port) == 0) {
						already_reg = 1;
						break;
					}
				} 
				printf("ALR: %i\n", already_reg);
				
				//controllo sul numero di peer registrati
				if((num_peer == MAX_PEER) && (already_reg == 0)) {
					printf("Errore: raggiunto il numero massimo di peer\n");
					sprintf(buffer, "%s", "MAX_EXC");
					sendToPeer(connecting_addr);

				} else {	//c'è ancora spazio nel buffer
					
					if(already_reg == 0) {
						//registrazione del nuovo peer
						for(i = 0; i < num_peer; i++) {
							if ((num_peer == 0) || (atoi(second_arg) < atoi(peer_registered[i].port)))
								break;
						}  		//i è l'indice in cui inserire il nuovo peer

						// faccio shift
						// (inserimento tra due peer)
						if(i < num_peer) {
							for(j = num_peer-1; j >= i; j--) {
								peer_registered[j+1] = peer_registered[j];
								peer_addr[j+1] = peer_addr[j];
							}
						}
			
						//altrimenti aggiungo di seguito
						peer_addr[i] = connecting_addr;

						strcpy(peer_registered[i].ip, first_arg);
						strcpy(peer_registered[i].port, second_arg);

						num_peer++;

						printf("NUM_PEER: %i\n", num_peer);
					}

					//comunico registrazione avvenuta
					printf("Peer registrato\n");
					sprintf(buffer, "%s", "ACK");
					sendToPeer(connecting_addr);

					//invio le informazioni sui vicini al nuovo peer
					if(num_peer <= 2) {

						if(num_peer == 1) {
							sprintf(buffer, "NEIGHBORS - - - -");
							printf("%s\n", buffer);
							sendToPeer(peer_addr[0]);
						}

						if (num_peer == 2) {	//un solo vicino
							
							if(i == 1) {	//secondo peer in posizione 1
								if(already_reg == 0) {
									//aggiorno i vicini di 0
									strcpy(right_ip, peer_registered[1].ip);
									strcpy(right_port, peer_registered[1].port);
									sprintf(buffer, "NEIGHBORS - - %s %s", right_ip, right_port);
									printf("Aggiorno vicini di %s: %s\n", peer_registered[0].port, buffer);
									sendToPeer(peer_addr[0]);
								}
								//invio a 1 il vicino 0
								strcpy(left_ip, peer_registered[0].ip);
								strcpy(left_port, peer_registered[0].port);
								sprintf(buffer, "NEIGHBORS %s %s - -", left_ip, left_port);
								printf("Invio vicini %s: %s\n", peer_registered[1].port, buffer);
								sendToPeer(peer_addr[1]);
							} else {		//nuovo peer in prima posizione
								
								if(already_reg == 0) {
									//aggiorno i vicini di 1
									strcpy(left_ip, peer_registered[0].ip);
									strcpy(left_port, peer_registered[0].port);
									sprintf(buffer, "NEIGHBORS %s %s - -", left_ip, left_port);
									printf("Invio vicini %s: %s\n", peer_registered[1].port, buffer);
									sendToPeer(peer_addr[1]);
								}

								//invio a 0 il vicino 1
								strcpy(right_ip, peer_registered[1].ip);
								strcpy(right_port, peer_registered[1].port);
								sprintf(buffer, "NEIGHBORS - - %s %s", right_ip, right_port);
								printf("Aggiorno vicini di %s: %s\n", peer_registered[0].port, buffer);
								sendToPeer(peer_addr[0]);							
							}

						}

					} else if(num_peer > 2) {

						if(already_reg == 0) {
							//comunico ai vicini di i il nuovo vicino
							for(j = 0; j < num_peer; j++) {	//cerco i vicini del peer inserito

								if((j == ((i+num_peer-1)%num_peer)) || (j == ((i+1)%num_peer))) {
								
									strcpy(left_ip, peer_registered[(j+num_peer-1)%num_peer].ip);
									strcpy(left_port, peer_registered[(j+num_peer-1)%num_peer].port);
									strcpy(right_ip, peer_registered[(j+1)%num_peer].ip);
									strcpy(right_port, peer_registered[(j+1)%num_peer].port);
									sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);
								
									printf("Aggiorno i vicini di %s: %s\n", peer_registered[j].port, buffer);
									sendToPeer(peer_addr[j]);
								}
							}
						}
						//invio al nuovo peer i propri vicini
						strcpy(left_ip, peer_registered[(i-1+num_peer)%num_peer].ip);
						strcpy(left_port, peer_registered[(i-1+num_peer)%num_peer].port);
						strcpy(right_ip, peer_registered[(i+1)%num_peer].ip);
						strcpy(right_port, peer_registered[(i+1)%num_peer].port);
						sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);

						printf("Invio vicini a %s: %s\n", peer_registered[i].port, buffer);
						sendToPeer(connecting_addr);
						
					} //else > 2
				} //else -> registrazione					
			} //fine BOOT

			if(strcmp(command, "QUIT") == 0) {
				//cerco il peer interessato
				for(i = 0; i < num_peer; i++)
					if((strcmp(peer_registered[i].ip, first_arg) == 0) && 
					   (strcmp(peer_registered[i].port, second_arg) == 0))
					   break;
					
				if(num_peer == 1)
					num_peer--;

				if(num_peer > 1) {		//se ce n'è solo uno non faccio nulla, altrimenti
					if(num_peer == 2) {

						if (i == 0) {	//eliminazione in testa
							peer_registered[i] = peer_registered[1];
							peer_addr[0] = peer_addr[1];
						}
						num_peer--;
						sprintf(buffer, "NEIGHBORS - - - -");
						printf("Aggiorno i vicini di %s: %s\n", peer_registered[0].port, buffer);
						sendToPeer(peer_addr[0]);							
						
					} else if(num_peer > 2){

						if(i == 0) {	//estrazione in testa

							for(j = 0; j < num_peer; j++) {
								//shift a sinistra
								peer_addr[j] = peer_addr[j+1];
								peer_registered[j] = peer_registered[j+1];
							}

							num_peer--;

							//aggiorno nuovo in testa
							if(num_peer == 2) {
								strcpy(left_ip, "-");
								strcpy(left_port, "-");
							} else {
								strcpy(left_ip, peer_registered[num_peer-1].ip);
								strcpy(left_port, peer_registered[num_peer-1].port);
							}
							strcpy(right_ip, peer_registered[1].ip);
							strcpy(right_port, peer_registered[1].port);
							sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);
							
							printf("Invio vicini a %s: %s\n", peer_registered[0].port, buffer);
							sendToPeer(peer_addr[0]);

							//aggiorno l'ultimo in coda
							strcpy(left_ip, peer_registered[num_peer-2].ip);
							strcpy(left_port, peer_registered[num_peer-2].port);
							if(num_peer == 2) {
								strcpy(right_ip, "-");
								strcpy(right_port, "-");
							} else {
								strcpy(right_ip, peer_registered[0].ip);
								strcpy(right_port, peer_registered[0].port);
							}
							sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);

							printf("Invio vicini a %s: %s\n", peer_registered[num_peer-1].port, buffer);
							sendToPeer(peer_addr[num_peer-1]);
							
						} else if (i == (num_peer - 1)) {	//eliminazione in coda
					
							num_peer--;
							//aggiornamento nuovo ultimo
							strcpy(left_ip, peer_registered[num_peer-2].ip);
							strcpy(left_port, peer_registered[num_peer-2].port);
							if(num_peer == 2) {
								strcpy(right_ip, "-");
								strcpy(right_port, "-");
							} else {
								strcpy(right_ip, peer_registered[0].ip);
								strcpy(right_port, peer_registered[0].port);
							}
							sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);
							
							printf("Invio vicini a %s: %s\n", peer_registered[num_peer-1].port, buffer);
							sendToPeer(peer_addr[num_peer-1]);

							//aggiornamento vicini del primo
							if(num_peer == 2) {
								strcpy(left_ip, "-");
								strcpy(left_port, "-");
							} else {
								strcpy(left_ip, peer_registered[num_peer-1].ip);
								strcpy(left_port, peer_registered[num_peer-1].port);
							}
							strcpy(right_ip, peer_registered[1].ip);
							strcpy(right_port, peer_registered[1].port);
							
							sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);

							printf("Invio vicini a %s: %s\n", peer_registered[0].port, buffer);
							sendToPeer(peer_addr[0]);
					
						} else {	//estrazione in mezzo
							
							num_peer--;

							for(j = i; j < num_peer; j++) {
								//shift a sinistra
								peer_addr[j] = peer_addr[j+1];
								peer_registered[j] = peer_registered[j+1];
							}							

							//aggiornamento peer a sinistra
							if(num_peer == 2) {
								strcpy(left_ip, "-");
								strcpy(left_port, "-");	
							} else {
								strcpy(left_ip, peer_registered[(i-2+num_peer)%num_peer].ip);
								strcpy(left_port, peer_registered[(i-2+num_peer)%num_peer].port);
							}
							strcpy(right_ip, peer_registered[i].ip);
							strcpy(right_port, peer_registered[i].port);
							
							sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);

							printf("Invio vicini a %s: %s\n", peer_registered[(i-1+num_peer)%num_peer].port, buffer);
							sendToPeer(peer_addr[(i-1+num_peer)%num_peer]);

							//aggiornamento peer a destra
							strcpy(left_ip, peer_registered[i-1].ip);
							strcpy(left_port, peer_registered[i-1].port);
							if(num_peer == 2) {
								strcpy(right_ip, "-");
								strcpy(right_port, "-");
							} else {
								strcpy(right_ip, peer_registered[(i+1)%num_peer].ip);
								strcpy(right_port, peer_registered[(i+1)%num_peer].port);
							}
							sprintf(buffer, "NEIGHBORS %s %s %s %s", left_ip, left_port, right_ip, right_port);

							printf("Invio vicini a %s: %s\n", peer_registered[i].port, buffer);
							sendToPeer(peer_addr[i]);
							
						}
					}
				}
			}

			if(strcmp(command, "SOME_ENTRIES") == 0) {
			
				DS_entry.num_peer_N += atoi(first_arg);
				DS_entry.num_peer_T += atoi(second_arg);
				counter++;

				if(counter == num_peer) {
					counter = 0;
					updateRegister();
				}
			}

			if(strcmp(command, "GET") == 0) {
			
				//conversione date da cercare
				if(strcmp(second_arg, "*") != 0) {
					strptime(second_arg, "%d:%m:%Y", &dateToConvert);
					min_date_given = mktime(&dateToConvert)+TZ;
				}
				if(strcmp(third_arg, "*") != 0) {
					strptime(third_arg, "%d:%m:%Y", &dateToConvert);
					max_date_given = mktime(&dateToConvert)+TZ;
				}

				fclose(fd);
				fd = fopen(filepath, "r");	//apro il file in lettura
				if(fd != NULL) {	
					sprintf(buffer, "RESPONSE");

					//caso *,*
					if((strcmp(second_arg, "*") == 0) && (strcmp(third_arg, "*") == 0)) {
						while(fscanf(fd, "%s %i %i\n", &findEntry.date, 
							&findEntry.num_peer_N, &findEntry.num_peer_T) != EOF)
							
							sprintf(buffer + strlen(buffer), "-%s %i %i", 
									findEntry.date, findEntry.num_peer_N, findEntry.num_peer_T);
					}	

					//caso d:m:Y - *
					if(strcmp(third_arg, "*") == 0) {

						while(fscanf(fd, "%s %i %i\n", &findEntry.date, &findEntry.num_peer_N, 
									&findEntry.num_peer_T) != EOF) {
							
							//conversione data prelevata
							strptime(findEntry.date, "%d:%m:%Y", &dateToConvert);
							date_tmp = mktime(&dateToConvert)+TZ;

							if(difftime(min_date_given, date_tmp) <= 0) {
								sprintf(buffer + strlen(buffer), "-%s %i %i", findEntry.date, 
										findEntry.num_peer_N, findEntry.num_peer_T);
							}
						}
					}

					//caso * - d:m:Y
					if(strcmp(second_arg, "*") == 0) {
						while(fscanf(fd, "%s %i %i\n", &findEntry.date, &findEntry.num_peer_N, 
									&findEntry.num_peer_T) != EOF) {
							
							//conversione data prelevata
							strptime(findEntry.date, "%d:%m:%Y", &dateToConvert);
							date_tmp = mktime(&dateToConvert)+TZ;
							if(difftime(max_date_given, date_tmp) >= 0) 
							{
								sprintf(buffer + strlen(buffer), "-%s %i %i", findEntry.date, 
										findEntry.num_peer_N, findEntry.num_peer_T);
							}
						}
					}

					//caso d:m:Y - d:m:Y
					if((strcmp(second_arg, "*") != 0) && (strcmp(third_arg, "*") != 0)) {
						printf("CASO data - data\n");
						while(fscanf(fd, "%s %i %i\n", &findEntry.date, &findEntry.num_peer_N, 
									&findEntry.num_peer_T) != EOF) {
							
							//conversione data prelevata
							strptime(findEntry.date, "%d:%m:%Y", &dateToConvert);
							date_tmp = mktime(&dateToConvert)+TZ;
							if((difftime(min_date_given, date_tmp) <= 0) && 
							(difftime(max_date_given, date_tmp) >= 0)) 
							{
								sprintf(buffer + strlen(buffer), "-%s %i %i", findEntry.date, 
										findEntry.num_peer_N, findEntry.num_peer_T);
							}
						}
					}
					sendToPeer(connecting_addr);
					printf("Invio: %s\n", buffer);
					fclose(fd);
					fopen(filepath, "a");
				}
			}
		} //fine FD_ISSET

		if (FD_ISSET(0, &read_fds)) {  	//stdin pronto in lettura
			scanf("%[^\n]", buffer);
			scanf("%*c");
		
			howmany = parse_string(buffer);

			if((strcmp(command, "help") == 0) && (valid_input == 1)) {  printf("DENTRO HELP\n");
				printf("\nDettaglio comandi:\n");
				printf("1) help 		--> mostra i dettagli dei comandi\n");
				printf("2) showpeers 		--> mostra un elenco dei peer connessi\n");
				printf("3) showneighbor <peer> 	--> mostra i neighbor di un peer\n");
				printf("4) esc 			--> chiude il DS\n\n");
			}

			if((strcmp(command, "showpeers") == 0) && (valid_input == 1)) {
				printf("Peer connessi: ");
				for(i = 0; i < num_peer; i++) {
					printf("%s ", peer_registered[i].port);
				}
				printf("\n");
			}

			if((strcmp(command, "showneighbor") == 0) && (valid_input == 1)) {
				if(howmany == 1){	//neighbors di tutti i peer
					if(num_peer == 0) {
						printf("No peer registered\n");
					} else {
						for(i = 0; i < num_peer; i++) {
							printf("Peer: %s\t", peer_registered[i].port);
							printf("Neighbors: ");

							if(num_peer == 1) 
								printf("- - , - - \n");
							
							if(num_peer == 2) {
								if(i == 0) {
									printf("- - , ");
									printf("%s ", peer_registered[1].ip);
									printf("%s \n", peer_registered[1].port);
								} else if (i == 1){
									printf("%s ", peer_registered[0].ip);
									printf("%s,", peer_registered[0].port);
									printf(" - -\n");
								}
							} else if (num_peer > 2) {
								printf("%s ", peer_registered[(i+num_peer-1)%num_peer].ip);
								printf("%s, ", peer_registered[(i+num_peer-1)%num_peer].port);
								printf("%s ", peer_registered[(i+1)%num_peer].ip);
								printf("%s \n", peer_registered[(i+1)%num_peer].port);
							}
						}
					}
				} else { //neighbor di un peer specificato
					for(i = 0; i < num_peer; i++) {
						if((strcmp(peer_registered[i].port, first_arg)) == 0) {	//cerco il peer
							printf("Peer: %s\t", peer_registered[i].port);
							printf("Neighbors: ");
							if(num_peer == 1)
								printf("- - , - - \n");
							if(num_peer == 2) {
								if(i == 0) {
									printf(" - - , ");
									printf("%s ", peer_registered[1].ip);
									printf("%s \n", peer_registered[1].port);
								} else if (i == 1){
									
									printf("%s ", peer_registered[0].ip);
									printf("%s, ", peer_registered[0].port);
									printf("- -\n");
								}
							} 
							else if(num_peer > 2) {
								printf("%s ", peer_registered[(i+num_peer-1)%num_peer].ip);
								printf("%s, ", peer_registered[(i+num_peer-1)%num_peer].port);
								printf("%s ", peer_registered[(i+1)%num_peer].ip);
								printf("%s \n", peer_registered[(i+1)%num_peer].port);
							}
						}
					}
				}
			}

			if((strcmp(command, "esc") == 0) && (valid_input == 1)) {
				sprintf(buffer, "%s", "ESC");
				len = strlen(buffer) + 1;
				//comunico a tutti i peer registrati la terminazione
				for(i = 0; i < num_peer; i++) 
					sendToPeer(peer_addr[i]);
				
				closing_actions();
				exit(0);
			}			
		} //FD_ISSET
	} //while

	closing_actions();
    
} //main

