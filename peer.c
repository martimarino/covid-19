
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
char* peer_port;
//peer_port = malloc(ADDR_LEN-1);
struct sockaddr_in srv_addr, my_addr;
char buffer[BUFFER_LEN];

// comando da inviare al server
char command[CMD_LEN];

// informazioni da inviare al server
char info[BUFFER_LEN];

struct Boot boot_msg;
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

	// Estraggo il numero di porta
	if(argc == 1) {
		printf("Comando non riconosciuto: inserire numero di porta\n");
		exit(1);
	}
	if(argc > 2) {
		printf("Comando non riconosciuto\n");
		exit(1);
	}
	if(argc == 2)
		peer_port = argv[argc-1];
		
	
	peer_connect();

    do {
		// compilo la struct req
		strcpy(req.cmd, "BOOT\0"); 
		req.howmany = ADDR_LEN + PORT_LEN +1;	//16+7+virgola

		//copio la struct nel buffer command da inviare
		sprintf(command, "%s,%d", req.cmd, req.howmany);

		printf("COMMAND: %s\n", command);
        // Tento di fare il boot continuamente        
        ret = sendto(sd, command, sizeof(command), 0,
                       (struct sockaddr*)&srv_addr, sizeof(srv_addr));
        // Se la richiesta non e' stata inviata vado a dormire per un poco
        if (ret < 0)
                sleep(POLLING_TIME);
    } while (ret < 0);
printf("Comando di boot inviato\n");
	do {
		//compilo la struct boot_msg
		strcpy(boot_msg.ip, localhost);
		strcpy(boot_msg.port, peer_port);

		//copio la struct nel buffer command da inviare
		sprintf(info, "%s,%s", boot_msg.ip, boot_msg.port);

		printf("INFO: %s\n", info);
        // Tento di inviare le informazioni di boot continuamente        
        ret = sendto(sd, info, sizeof(info), 0,
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

	//free(peer_port);
    printf("%s\n", buffer);

    close(sd);
}
