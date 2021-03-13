
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "shared.h"

#define MAX_PEER        10
#define POLLING_TIME    5       //controllo ogni 5 secondi

struct Boot peer_boot;
struct Request peer_req;

int main(int argc, char* argv[]) {
    
    int num_peer = 0;   //numero peer registrati
    int i;
    int ret, sd, len, addrlen, newfd;
	char cmd[BUFFER_LEN];
	char *tok;		//puntatore per la strtok()

	fd_set master;		//set di tutti i descrittori
	fd_set read_fds;	//set dei descrittori in lettura
	int fdmax = 2;

    struct sockaddr_in my_addr;//, peer_addr;    //struct per indirizzi
    struct sockaddr_in peer_addr[MAX_PEER];   //array indirizzi registrati
    
    char buffer[BUFFER_LEN];
    
    time_t rawtime;

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
        exit(0);
    }
    
	//reset dei descrittori
	FD_ZERO(&master);			//svuota master
	FD_ZERO(&read_fds);			//svuota read_fds

	FD_SET(0, &master);			//aggiunge stdin a master
	FD_SET(sd, &master);		//aggiunge sd a master

	fdmax = sd+1;
	
        
    printf("******************* DS COVID STARTED *******************\n");
    printf("Digita un comando:\n");    
    
    while(1) {

		read_fds = master;  
		select(fdmax+1, &read_fds, NULL, NULL, NULL);	
		// select ritorna quando un descrittore è pronto
	
		if (FD_ISSET(sd, &read_fds)) {  //sd pronto in lettura
	
			addrlen = sizeof(peer_addr);
			ret = recvfrom(sd, buffer, CMD_LEN, 0,
		            (struct sockaddr*)&peer_addr, &addrlen);
			if(ret < 0) {
				perror("Errore richiesta dal peer\n");
				exit(1);
			}
			printf("COMMAND RICEVUTO: %s\n", buffer);
		
			sscanf(buffer, "%s,%d", &peer_req.cmd, &peer_req.howmany);
	
			if(strcmp(peer_req.cmd, "BOOT") == 0) {
			
				ret = recvfrom(sd, buffer, peer_req.howmany, 0,
				        (struct sockaddr*)&peer_addr, &addrlen);
				if(ret < 0) {
					perror("Errore richiesta dal peer\n");
					exit(1);
				}

				printf("BUFFER RICEVUTO: %s\n", buffer);

				sscanf(buffer, "%s,%s", &peer_boot.ip, &peer_boot.port);

				printf("Ho ricevuto la struct: %s\n", peer_boot.ip);
				time(&rawtime);
				sprintf(buffer, "%s", ctime(&rawtime));
				len = strlen(buffer) + 1;
				ret = sendto(sd, buffer, len, 0,
				     (struct sockaddr*)&peer_addr[i], sizeof(peer_addr[i]));
				if (ret < 0)
					perror("Errore invio risposta al peer\n");
			}

		}
		if (FD_ISSET(0, &read_fds)) {  	//stdin pronto in lettura

			scanf("%s", &cmd);
			//printf("%s", cmd);
			//printf("Comando ricevuto: %s\n", cmd);

			if(strcmp(cmd, "help") == 0) {
				printf("\nDettaglio comandi:\n");
				printf("1) help 		--> mostra i dettagli dei comandi\n");
				printf("2) showpeers 		--> mostra un elenco dei peer connessi\n");
				printf("3) showneighbor <peer> 	--> mostra i neighbor di un peer\n");
				printf("4) esc 			--> chiude il DS\n\n");
			}

			if(strcmp(cmd, "showpeers") == 0) {
				
			}

			if(strcmp(cmd, "there's a problem'") == 0) {
				
			}

			if(strcmp(cmd, "esc") == 0) {

				sprintf(buffer, "%s", "ESC");
				len = strlen(buffer) + 1;
				ret = sendto(sd, buffer, len, 0,
				     (struct sockaddr*)&peer_addr[i], sizeof(peer_addr[i]));

				close(sd);
				FD_CLR(sd, &master);
				
				close(0);
				FD_CLR(0, &master);

				exit(0);
			}			
		}
	}

/*
        //aspetto richieste di connessione
        do {
            ret = recvfrom(sd, buffer, REQ_LEN, 0,
                            (struct sockaddr*)&peer_addr, &addrlen);
            if(ret < 0)
                sleep(POLLING_TIME);

			scanf("%s", &cmd);
			if(strcmp(cmd, "help")) {
				printf("help - ricevuto\n");
			}
		
			if(strcmp(cmd, "showpeers")) {
				printf("showpeers - ricevuto\n");
			}
		
			if(strcmp(cmd, "esc")) {
				printf("esc - ricevuto\n");
			}
			tok = strtok(cmd, " ");
			if(strcmp(tok, "showneighbor"))	{
				tok = strtok(cmd, NULL);
				if(!tok) {
					printf("mostra i neighbor di tutti i peer");
				} else {
					printf("mostra i neighbor del peer specificato");
				}
			}
			printf("Comando non riconosciuto\n");

        } while (ret < 0);
        
        //aggiungo il nuovo client alla lista
        peer_addr[num_peer] = peer_addr;
        num_peer++;
        
        //richiedo l'ora corrente
        time(&rawtime);
        
        //inserisco l'ora nel buffer come stringa (sprintf aggiunge \0)
        sprintf(buffer, "%s", ctime(&rawtime));
        len = strlen(buffer) + 1;
        
        //notifico i peer registrati
        for (i = 0; i < num_peer; i++) {
            do {
                ret = sendto(sd, buffer, len, 0,
                         (struct sockaddr*)&peer_addr[i], sizeof(peer_addr[i]));
                if (ret < 0)
                    sleep(POLLING_TIME);
            } while (ret < 0);
        }
    }*/
    
    close(sd);
    
}

