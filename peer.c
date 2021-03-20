
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
#define ACK_LEN 	  3

int ret, sd, len, i;
char localhost[ADDR_LEN] = "127.0.0.1\0";
char peer_port[PORT_LEN];	//porta del peer attuale
char* tmp_port;				//variabile per prelevare porta da terminale

int peer_connected = 0;		//indica se il socket è stato creato
int peer_registered = 0;	//indica se il peer è registrato con un DS	


//variabili per prelevare i campi di un messaggio
char *token;				//per l'utilizzo di strtok
int valid_input = 1;		//indica se un comando ha il fomato corretto
char command[CMD_LEN+1];	//primo campo di un messaggio
char first_arg[BUFFER_LEN];
char second_arg[BUFFER_LEN];
char third_arg[BUFFER_LEN];
char fourth_arg[BUFFER_LEN];

struct sockaddr_in srv_addr, my_addr;
char buffer[BUFFER_LEN];

//variabili per select()
fd_set master;		//set di tutti i descrittori
fd_set read_fds;	//set dei descrittori in lettura
int fdmax = 0;

//variabili per il file del peer
FILE* fd;
char filepath[BUFFER_LEN];
char filename[] = "register.txt";

//variabili per il polling del boot
int nfds, num_open_fds, ready;
struct pollfd *pfds;

void closing_actions() {	//azioni da compiere quando un peer termina
	free(tmp_port);
	free(token);

	if(peer_connected == 1) {
		close(sd);
		printf("Chiusura del socket effettuata\n");
	}
	FD_CLR(0, &master);
	FD_CLR(sd, &master);
}

void peer_connect(char DS_addr[], char DS_port[]) {		//creazione del socket
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
        perror("Bind non riuscita ");
		if(peer_connected == 0) {	//se il peer è gia registrato non faexit
			closing_actions();
			exit(-1);
		}
    }
    /* Creazione indirizzo del server */
    memset (&srv_addr, 0, sizeof(srv_addr)); 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(DS_port));
    inet_pton(AF_INET, DS_addr, &srv_addr.sin_addr);

	fdmax = sd;
	peer_connected = 1;
	printf("Socket creato\n");
}

void parse_string(char buffer[]) {	//separa gli argomenti di un comando

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
			case 4:
				sscanf(token, "%s", &fourth_arg);
				break;
			default:
				printf("Comando non riconosciuto\n");
				break;
		
		}
		token = strtok(NULL, " ");
	}

//controllo del formato
	if(((strcmp(command, "start") == 0) && (i != 3)) ||
		((strcmp(command, "add") == 0) && (i != 2)) ||
		((strcmp(command, "get") == 0) && (i != 4)) ||
		((strcmp(command, "stop") == 0) && (i != 1))) {
		printf("Comando non riconosciuto\n");
		valid_input = 0;
	} else {
		valid_input = 1;
	}
}

int main(int argc, char* argv[]){

	tmp_port = (char*)malloc(sizeof(char)*ADDR_LEN);
	if(tmp_port == NULL) {
		perror("Memory not allocated: \n");
		exit(-1);
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
		perror("Comando non riconosciuto\n");
		exit(-1);
	}
	if(argc == 2){
		strcpy(tmp_port, argv[argc-1]);
		strcpy(peer_port, tmp_port);		
	}


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

			parse_string(buffer);

			if(strcmp(command, "ESC") == 0) {
				printf("Chiusura a causa dell terminazione del DS\n");
				closing_actions();
				exit(0);
			}

		}

		if (FD_ISSET(0, &read_fds)) {  	//stdin pronto in lettura
			
			scanf("%[^\n]", buffer);
			scanf("%*c");
			
			parse_string(buffer);
		


			if((strcmp(command, "start") == 0) && (valid_input == 1) && (peer_registered == 1)){
				printf("Errore: peer già registato presso il DS\n");
			}

			if((strcmp(command, "start") == 0) && (valid_input == 1) && (peer_registered == 0)){
				
				printf("Richiesta connessione al DS...\n");
				peer_connect(first_arg, second_arg);

				num_open_fds = nfds = 2;	
				pfds = calloc(nfds, sizeof(struct pollfd));
				if(pfds == NULL)
					perror("Allocazione pfds fallita");
				pfds[0].fd = 0;
				pfds[0].events = POLLIN;
				pfds[1].fd = sd;
				pfds[1].events = POLLIN;

				while(1) {

					do {
						//copio la struct nel buffer da inviare
						sprintf(buffer, "BOOT %s %s", localhost, peer_port);
						len = strlen(buffer)+1;
						printf("Boot message inviato: %s\n", buffer);
						// Tento di inviare le informazioni di boot continuamente        
						ret = sendto(sd, buffer, len, 0,
										(struct sockaddr*)&srv_addr, sizeof(srv_addr));
						// Se la richiesta non e' stata inviata vado a dormire per un poco
						if (ret < 0)
							sleep(POLLING_TIME);
						
					} while (ret < 0);

					ready = poll(pfds, nfds, 4000);

					if(ready) {
						if(pfds[1].revents) {	// riceve qualcosa da DS
							do {
								/* Tento di ricevere i dati dal server  */
								ret = recvfrom(sd, buffer, BUFFER_LEN, 0, 
												(struct sockaddr*)&srv_addr, &len);

								/* Se non ricevo niente vado a dormire per un poco */
								if (ret < 0)
									sleep(POLLING_TIME);

							} while(ret < 0);
							printf("Ho ricevuto: %s\n", buffer);

							parse_string(buffer);

							if(strcmp(command, "ACK") == 0) {
								peer_registered = 1;
								break;
							}

							if(strcmp(command, "MAX_EXC") == 0) {
								printf("Non è possibile registrarsi\n");
								closing_actions();
								exit(0);
							}
						}

						if(pfds[0].revents) {	// può ricevere stop da terminale
							
							scanf("%[^\n]", buffer);
							scanf("%*c");

							parse_string(buffer);
							
							if((strcmp(command, "stop") == 0) && (valid_input == 1)) {
								printf("Terminazione forzata\n");
								closing_actions();
								exit(0);
							}

						}
					}
				}
			
//printf("INFO BOOT INVIATE\n");

				printf("Recupero informazioni da file\n");

				strcpy(filepath, "./");
				strcat(filepath, peer_port);
				strcat(filepath, "/");
				strcat(filepath, filename);
				
				fd = fopen(filepath, "r+");
				if(fd == NULL) {	//se non esiste lo creo
					fd = fopen(filepath, "w");
					if(fd == NULL)
						perror("Error: ");
					else {
						fclose(fd);		//lo riapro in r/w
						fd = fopen(filepath, "r+");
					}
				}
				printf("Attendo risposta dal DS...\n");

			}
/*
			if((strcmp(command, "add") == 0) && (valid_input == 1)) {	
				
			}

			if((strcmp(command, "get") == 0) && (valid_input == 1)) {	

			}
*/	
			if((strcmp(command, "stop") == 0) && (valid_input == 1)) {	
			
				do {
					//invio 
					sprintf(buffer, "QUIT %s %s", localhost, peer_port);
					len = strlen(buffer)+1;
					printf("Comunicazione di terminazione al DS: %s\n", buffer);
					// Tento di inviare le informazioni di boot continuamente        
					ret = sendto(sd, buffer, len, 0,
									(struct sockaddr*)&srv_addr, sizeof(srv_addr));
					// Se la richiesta non e' stata inviata vado a dormire per un poco
					if (ret < 0)
								sleep(POLLING_TIME);
				} while (ret < 0);


				printf("Terminazione forzata\n");
				closing_actions();
				exit(0);
			}
		}	
	} //while

	closing_actions();
    printf("%s\n", buffer);

} //main


