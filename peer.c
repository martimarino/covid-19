
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

#define BUFFER_SIZE   1024
#define POLLING_TIME  5
#define RES_LEN       26    // Wed Dec 09 17:41:29 2020\0\n

int main(int argc, char* argv[]){
    int ret, sd, len;
    struct sockaddr_in srv_addr, my_addr;
    char buffer[BUFFER_SIZE];
    // Comando da inviare al server

    char* cmd = "REQ\0";  
    /* Creazione socket */
    sd = socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,0);
    /* Creazione indirizzo di bind */
    memset (&my_addr, 0, sizeof(my_addr));     // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4243);
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
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);
    do {
        // Tento di inviare la richiesta continuamente        
        ret = sendto(sd, cmd, sizeof(cmd), 0,
                       (struct sockaddr*)&srv_addr, sizeof(srv_addr));
        // Se la richiesta non e' stata inviata vado a dormire per un poco
        if (ret < 0)
                sleep(POLLING_TIME);
    } while (ret < 0);
    
    do {
        // Tento di ricevere i dati dal server  
        ret = recvfrom(sd, buffer, RES_LEN, 0, 
                        (struct sockaddr*)&srv_addr, &len);

        // Se non ricevo niente vado a dormire per un poco
        if (ret < 0)
            sleep(POLLING_TIME);
    } while(ret < 0);
    
    printf("%s\n", buffer);

    close(sd);
}
