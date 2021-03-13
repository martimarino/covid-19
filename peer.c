
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include "shared.h"

#define POLLING_TIME  5
#define RES_LEN       26    // Wed Dec 09 17:41:29 2020\0\n

int ret, sd, len;
char localhost[ADDR_LEN] = "127.0.0.1\0";
char peer_port[PORT_LEN];
char* tmp_port;

struct sockaddr_in srv_addr, my_addr;
char buffer[BUFFER_LEN];

// informazioni da inviare al server
char info[BUFFER_LEN];

struct Request req;

void peer_connect() {
    /* Creazione socket */
    sd = socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,0);

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
    srv_addr.sin_port = htons(4242);
    inet_pton(AF_INET, localhost, &srv_addr.sin_addr);
}

int main(int argc, char* argv[]){

	tmp_port = (char*)malloc(sizeof(char)*ADDR_LEN);
	if(peer_port == NULL) {
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
		tmp_port = argv[argc-1];
		strcpy(peer_port, tmp_port);		
		printf("NUMERO DI PORTA: %s\n", peer_port);
	}

	peer_connect();

	do {

		//copio la struct nel buffer da inviare
		sprintf(buffer, "BOOT,%s,%s", localhost, peer_port);

		printf("Boot message: %s\n", buffer);
        // Tento di inviare le informazioni di boot continuamente        
        ret = sendto(sd, buffer, sizeof(buffer), 0,
                       (struct sockaddr*)&srv_addr, sizeof(srv_addr));
        // Se la richiesta non e' stata inviata vado a dormire per un poco
        if (ret < 0)
                sleep(POLLING_TIME);
    } while (ret < 0);
printf("Info boot inviate\n");
    do {
        // Tento di ricevere i dati dal server  
        ret = recvfrom(sd, buffer, RES_LEN, 0, 
                        (struct sockaddr*)&srv_addr, &len);

        // Se non ricevo niente vado a dormire per un poco
        if (ret < 0)
            sleep(POLLING_TIME);
		if(strcmp(buffer, "ESC") == 0) {
			printf("Terminazione forzata\n");
			close(sd);
			exit(0);
		}
    } while(ret < 0);

	free(tmp_port);
    printf("%s\n", buffer);

    close(sd);
}
