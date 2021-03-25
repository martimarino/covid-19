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
char my_port[PORT_LEN];	//porta del peer attuale
char* tmp_port;				//variabile per prelevare porta da terminale
char peer_port[ADDR_LEN];

int peer_connected = 0;		//indica se il socket è stato creato
int peer_registered = 0;	//indica se il peer è registrato con un DS	
int first_peer = 0;			//primo a connettersi al DS

//variabili per prelevare i campi di un messaggio
char *token;				//per l'utilizzo di strtok
int valid_input = 1;		//indica se un comando ha il fomato corretto
int howmany;				//numero token (compreso cmd)
char command[CMD_LEN+1];	//primo campo di un messaggio
char first_arg[BUFFER_LEN];
char second_arg[BUFFER_LEN];
char third_arg[BUFFER_LEN];
char fourth_arg[BUFFER_LEN];

//variabili per prelevare i campi del periodo
int valid_period = 1;
struct Aggr {
	char aggr[DATE_LEN];
	char type[2];
	char p_date[DATE_LEN];
	char r_date[DATE_LEN];
} elab;

struct tm dateToConvert;
time_t past_date, recent_date;

struct sockaddr_in srv_addr, my_addr, addr;
char buffer[BUFFER_LEN];

//variabili per select()
fd_set master;		//set di tutti i descrittori
fd_set read_fds;	//set dei descrittori in lettura
int fdmax = 0;
struct timeval *timeout;

//variabili per il file del peer
FILE* fd;
char filepath[BUFFER_LEN];
char filename[DATE_LEN];

//variabili per il polling del boot
int nfds, num_open_fds, ready;
struct pollfd *pfds;

struct Neighbors {
	char left_neighbor_ip[ADDR_LEN];
	char left_neighbor_port[PORT_LEN];
	char right_neighbor_ip[ADDR_LEN];
	char right_neighbor_port[PORT_LEN];
} my_neighbors;

struct Results {
	char date[DATE_LEN];
	int numPeerN;
	int numPeerT;
} DS_info;

struct Info {
	int nuoviCasi;
	int tamponi;
} tot, tot_tmp, var_tmp;

struct Entry {
	char type[2];
	int quantity;
} my_entry, entry_tmp;

//variabili per la creazione della data odierna/del giorno successivo
time_t now;
struct tm *todayDateTime, *tomorrowDateTime, *tmpDate;
char timeToCheck[6];
int monthDays, day, month, year;
char dd[3], mm[3], YY[5];

//variabili per la ricerca locale
FILE* fd_tmp;
char filepath_tmp[BUFFER_LEN];
char filename_tmp[DATE_LEN];
char inizio_pandemia[DATE_LEN];
time_t minDate, maxDate, beginningDate;
int flooding, found;
struct tm *nextDate;
char filename_prec[DATE_LEN];
char dateInterval[DATE_LEN+DATE_LEN];

struct Cache {
	char aggr[BUFFER_LEN];
	char type[BUFFER_LEN];
	char p_date[BUFFER_LEN];
	char r_date[BUFFER_LEN];
	char date[DATE_LEN];
	int result;
} cache_entry;

//variabili flooding
char buffer_tmp[BUFFER_LEN];
char side[2];
char cacheTotale[] = "totale.txt";
char cacheVariazione[] = "variazione.txt";


void closing_actions() {	//azioni da compiere quando un peer termina
	free(tmp_port);
	free(token);
	free(timeout);

	if(peer_connected == 1) {
		close(sd);
		fclose(fd);
		printf("Chiusura del socket effettuata\n");
	}
	FD_CLR(0, &master);
	FD_CLR(sd, &master);
}

void connect_to_peer(char peer_addr[], char peer_port[]) {

    /* Creazione indirizzo del peer da contattare */
    memset (&peer_addr, 0, sizeof(peer_addr)); 
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(peer_port));
    inet_pton(AF_INET, peer_addr, &addr.sin_addr);	
}

void connect_to_DS(char DS_addr[], char DS_port[]) {		//creazione del socket
    /* Creazione socket */
    sd = socket(AF_INET, SOCK_DGRAM, 0);

    /* Creazione indirizzo di bind */
    memset (&my_addr, 0, sizeof(my_addr));     // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(my_port));
    my_addr.sin_addr.s_addr = INADDR_ANY;

    /* Aggancio */
    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret < 0)
        perror("Bind non riuscita ");
    
    /* Creazione indirizzo del server */
    memset (&srv_addr, 0, sizeof(srv_addr)); 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(DS_port));
    inet_pton(AF_INET, DS_addr, &srv_addr.sin_addr);

	fdmax = sd;
	peer_connected = 1;
	printf("Socket creato\n");
}

void send_(struct sockaddr_in a) {
	do {
		len = strlen(buffer)+1;
		printf("Richiesta entry al DS: %s\n", buffer);       
		ret = sendto(sd, buffer, len, 0,
						(struct sockaddr*)&a, sizeof(a));
		if (ret < 0)
			sleep(POLLING_TIME);
	} while (ret < 0);
}

void receive_(struct sockaddr_in s) {
	//riceve comandi dal server
	do {
		/* Tento di ricevere i dati dal server  */
		ret = recvfrom(sd, buffer, BUFFER_LEN, 0, 
						(struct sockaddr*)&s, &len);
		if (ret < 0)
			sleep(POLLING_TIME);

	} while(ret < 0);
	printf("Ricevuto: %s\n", buffer);
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
		((strcmp(command, "add") == 0) && (i != 3)) ||
		((strcmp(command, "get") == 0) && (i != 4)) ||
		((strcmp(command, "stop") == 0) && (i != 1))) {
		printf("Comando non riconosciuto\n");
		valid_input = 0;
	} else {
		valid_input = 1;
	}
	return i;
}

int parse_period(char buffer[]) {	//separa le date del periodo

	token = strtok(buffer, "-");

	for(i = 0; token != NULL; i++) {
		
		switch(i) {
			case 0:
				sscanf(token, "%s", &elab.p_date);
				break;
			case 1:
				sscanf(token, "%s", &elab.r_date);
				break;
			default:
				printf("Troppe date inserite\n");
				break;
		}
		token = strtok(NULL, " ");
	}

	strptime(elab.p_date, "%d:%m:%Y", &dateToConvert);
	past_date = mktime(&dateToConvert);
	strptime(elab.r_date, "%d:%m:%Y", &dateToConvert);
	recent_date = mktime(&dateToConvert);

	if(strcmp(elab.p_date, "*") == 0 && (strcmp(elab.r_date, "*") == 0))
		return 1;

	if(strcmp(elab.p_date, "*") == 0 && (strcmp(elab.r_date, "*") != 0)) {
		if((difftime(recent_date, now) > 0) || ((strcmp(filename, elab.r_date) == 0)) 
			&& (inTime() == 1))
		{
			printf("Errore: registro giornaliero ancora aperto\n");
			return 0;
		}
		return 1;
	}

	if(strcmp(elab.p_date, "*") != 0 && (strcmp(elab.r_date, "*") == 0)) {
		if(difftime(past_date, beginningDate) < 0) {
			printf("Errore: data precedente a inizio pandemia\n");
			return 0;
		}
		if((strcmp(filename, elab.p_date) == 0) && (inTime() == 1))
		{
			printf("Errore: registro giornaliero ancora aperto\n");
			return 0;
		}
		return 1;
	}

	//controllo validità periodo
	if((difftime(recent_date, past_date) <= 0) || 
		(difftime(recent_date, now) > 0) ||
		(difftime(recent_date, past_date) == 0)) {
		printf("Periodo non valido\n");
		return 0;
	} else {
		if(((strcmp(filename, elab.r_date) == 0) || (strcmp(filename, elab.p_date) == 0)) 
			&& (inTime() == 1)) 
		{
			printf("Errore: registro giornaliero ancora aperto\n");
			return 0;
		}
		return 1;
	}
}

void recoverPreviousData() {

	fd = fopen(filepath, "r");	//apro il file in lettura
	if(fd != NULL) {		//se esisteva fopen ha successo
		while(fscanf(fd, "%s %i\n", &my_entry.type, &my_entry.quantity) != EOF) {
//printf("RECOVERED: %s %i\n", my_entry.type, my_entry.quantity);
			if(strcmp(my_entry.type, "N") == 0)
				tot.nuoviCasi += my_entry.quantity;
			if(strcmp(my_entry.type, "T") == 0)
				tot.tamponi += my_entry.quantity;
		}
		fclose(fd);
	}
}

void updateRegister() {	//salva dati odierni su file
	if(atoi(second_arg) == 0)
		printf("La quantità deve essere un intero\n");
	else{
		fprintf(fd, "%s %i\n", first_arg, atoi(second_arg));
	}
}
/*
void writeTotal() {
	strcpy(tot.str, "TOTALE");
	fprintf(fd, "%s %i %i //nuovi casi, tamponi\n", tot.str, tot.nuoviCasi, tot.tamponi);
	printf("Registro giornaliero chiuso\n");
}
*/
void communicateToDS() {
	sprintf(buffer, "SOME_ENTRIES %i %i", (tot.nuoviCasi>0)?1:0, (tot.tamponi>0)?1:0);
	send_(srv_addr);
	tot.nuoviCasi = 0;
	tot.tamponi = 0;
}

int daysInAMonth(int m) {
	if(m >= 1 && m <= 12) {
		if(m == 2)
			return 28;
		if(m == 4 || m == 6 || m == 9 || m == 11)
			return 30;
		return 31;
	}
	return 0;
}

struct tm* nextDay (struct tm *today) {
	tmpDate = localtime(&now);
	strftime(dd, sizeof(dd), "%d", today);
	strftime(mm, sizeof(mm), "%m", today);
	strftime(YY, sizeof(YY), "%Y", today);
	tmpDate->tm_sec = 0;
	tmpDate->tm_min = 0;
	tmpDate->tm_hour = 0;

	monthDays = daysInAMonth(atoi(mm)); 
	if(tmpDate->tm_mday != monthDays) {
		tmpDate->tm_mday = atoi(dd)+1;		
		tmpDate->tm_mon = atoi(mm)-1;  
		tmpDate->tm_year = atoi(YY)-1900;  
	}
	if(tmpDate->tm_mday == monthDays+1) {
		tmpDate->tm_mday = 1;
		tmpDate->tm_mon = atoi(mm);
		tmpDate->tm_year = atoi(YY)-1900;
	}
	if((tmpDate->tm_mon == 11) && (tmpDate->tm_mday-1 == 31)) {
		tmpDate->tm_mday = 1;
		tmpDate->tm_mon = 0;
		tmpDate->tm_year = atoi(YY)+1-1900;
	}
	return tmpDate;
}

int inTime() {
	time(&now);
	todayDateTime = localtime(&now);
	strftime(timeToCheck, sizeof(filename), "%R", todayDateTime);
	if(strcmp(timeToCheck, "18:00\0") < 0) 
		return 1;
	else 
		return 0;
}

void createFilePath(char path[], char name[]) {
	strcpy(path, "./");
	strcat(path, my_port);
	strcat(path, "/");
	strcat(path, name);
}

void createRegisterName() {
	if(inTime() == 0) {
		tomorrowDateTime = nextDay(todayDateTime); 
		strftime(filename, sizeof(filename), "%d:%m:%Y", tomorrowDateTime);
	} else {
		strftime(filename, sizeof(filename), "%d:%m:%Y", todayDateTime);
	}
	printf("FILENAME: %s\n", filename);
	createFilePath(filepath, filename);
}

void checkTime() {	//controlla se bisogna chiudere il file
	
	time(&now);
	todayDateTime = localtime(&now);
	strftime(timeToCheck, sizeof(filename), "%R", todayDateTime);

	if(strcmp(timeToCheck, "18:00\0") == 0) {
		printf("Time's over: %s\n", timeToCheck);
		//writeTotal();
		fclose(fd);
		communicateToDS();
		printf("Registro della data odierno chiuso\n");
		
		tomorrowDateTime = nextDay(todayDateTime); 
		strftime(filename, sizeof(filename), "%d:%m:%Y", tomorrowDateTime);

//printf("FILENAME: %s\n", filename);

		createFilePath(filepath, filename);		
		//apre file giorno successivo
		fd = fopen(filepath, "a");
		if(fd == NULL)
			perror("Error: ");
	}
	else {
		//printf("Not yet: %s\n", timeToCheck);
	}
}

void saveInCache (char cache[], char date[], int quantity) {
	createFilePath(filepath, cache);
	fd = fopen(filepath, "a");
	if(fd) {
		if(strcmp(cache, cacheTotale) == 0)
			fprintf(fd, "%s %s %s %s %i\n", elab.aggr, elab.type, elab.p_date,
					elab.r_date, quantity);
		if(strcmp(cache, cacheVariazione) == 0)
			fprintf(fd, "%s %s %s %s %s %i\n", elab.aggr, elab.type, elab.p_date,
					elab.r_date, date, quantity);
		fclose(fd);
	}
}

int searchInCache(char cache[], int caso) {	
	createFilePath(filepath, cache);
	fd = fopen(filepath, "r");
	if(fd) {
		if(strcmp(cache, cacheTotale) == 0) {
			while(fscanf(fd, "%s %s %s %s %i\n", cache_entry.aggr, 
					cache_entry.type, cache_entry.p_date, 
					cache_entry.r_date, cache_entry.result) != EOF) 
			{
				if((strcmp(cache_entry.aggr, elab.aggr) == 0) &&
					(strcmp(cache_entry.type, elab.type) == 0) &&
					(strcmp(cache_entry.p_date, elab.p_date) == 0) &&
					(strcmp(cache_entry.r_date, elab.r_date) == 0)) 
				{
					switch(caso) {
						case 0:
							//stampo a video
							printf("TOTALE: %i\n", cache_entry.result);
							fclose(fd);
							return 1;
						case 1:
							//stampo a video
							sprintf(buffer + strlen(buffer), "%i", cache_entry.result);
							fclose(fd);
							return 1;
						case 2:
							sprintf(buffer+strlen(buffer), " %s", my_port);
							fclose(fd);
							return 1;
						default:
							break;
					}
				}
			}
		}
		if(strcmp(cache, cacheVariazione) == 0) {
			found = 0;
			while(fprintf(fd, "%s %s %s %s %s %i\n", cache_entry.aggr,
				cache_entry.type, cache_entry.p_date, cache_entry.r_date,
				cache_entry.date, cache_entry.result) != EOF) 
			{
				if((strcmp(cache_entry.aggr, elab.aggr) == 0) &&
					(strcmp(cache_entry.type, elab.type) == 0) &&
					(strcmp(cache_entry.p_date, elab.p_date) == 0) &&
					(strcmp(cache_entry.r_date, elab.r_date) == 0)) 
				{
					switch(caso) {
						case 0:
							found = 1;
							printf("Variazione: %s %i\n", cache_entry.date, cache_entry.result);
							fclose(fd);
						case 1:
							found = 1;
							sprintf(buffer+strlen(buffer), " %s %i", cache_entry.date, cache_entry.result);
							fclose(fd);
						case 2:
							sprintf(buffer+strlen(buffer), " %s", my_port);
							fclose(fd);
							return 1;
						default:
							break;
					}
				}
			}
			if(found == 1) {
				fclose(fd);
				return 1;
			}	
		}
		fclose(fd);
	}
	return 0;
}

void getLocalTotal() {
	tot_tmp.nuoviCasi = 0;
	tot_tmp.tamponi = 0;
	while(difftime(minDate, maxDate) <= 0) {
		fd_tmp = fopen(filepath_tmp, "r");
		if(fd_tmp != NULL) {
			while(fscanf(fd_tmp, "%s %i\n",				//per tutti i dati del file
					&entry_tmp.type, &entry_tmp.quantity) != EOF) 
			{
				if(strcmp(entry_tmp.type, "N") == 0)
					tot_tmp.nuoviCasi += entry_tmp.quantity;
				if(strcmp(elab.type, "T") == 0)
					tot_tmp.tamponi += entry_tmp.quantity;
			}
			fclose(fd_tmp);
		}
		strptime(filename_tmp, "%d:%m:%Y", &dateToConvert);
		nextDate = &dateToConvert;
		nextDate = nextDay(nextDate);
		dateToConvert = *nextDate;
		minDate = mktime(&dateToConvert);
		strftime(filename_tmp, sizeof(filename_tmp), "%d:%m:%Y", &dateToConvert);
		//aggiorno filepath
		createFilePath(filepath_tmp, filename_tmp);
printf("NUOVO FILENAME %s\n", filename_tmp);
	}
	printf("TOTALE: ");
	if(strcmp(elab.type, "N") == 0) {
		printf("%i\n", tot_tmp.nuoviCasi);
		saveInCache(cacheTotale, "", tot_tmp.nuoviCasi);
	}
	if(strcmp(elab.type, "T") == 0) {
		printf("%i\n", tot_tmp.tamponi);
		saveInCache(cacheTotale, "", tot_tmp.tamponi);
	}	
}

void localVariation(){
	printf("VARIAZIONE: \n");
	tot_tmp.nuoviCasi = -1;
	tot_tmp.tamponi = -1;
	var_tmp.nuoviCasi = 0;
	var_tmp.tamponi = 0;
	strcpy(filename_prec, filename_tmp);
	while(difftime(minDate, maxDate) <= 0) {
		fd_tmp = fopen(filepath_tmp, "r");
		if(fd_tmp != NULL) {
			tot_tmp.nuoviCasi = var_tmp.nuoviCasi;	
			tot_tmp.tamponi = var_tmp.tamponi; 
			while(fscanf(fd_tmp, "%s %i\n",				//per tutti i dati del file
					&entry_tmp.type, &entry_tmp.quantity) != EOF) 
			{
				if(strcmp(entry_tmp.type, "N") == 0)
					var_tmp.nuoviCasi += entry_tmp.quantity;
				if(strcmp(elab.type, "T") == 0)
					var_tmp.tamponi += entry_tmp.quantity;
			}
			fclose(fd_tmp);
		}
		else { 
			var_tmp.nuoviCasi = 0;
			var_tmp.tamponi = 0;
		}

		if(!((tot_tmp.nuoviCasi == tot_tmp.tamponi) && (tot_tmp.nuoviCasi == -1))){
			sprintf(dateInterval, "%s-%s", filename_prec, filename_tmp);
			if(strcmp(elab.type, "N") == 0) {
				printf("%s: %i\n", dateInterval, tot_tmp.nuoviCasi-var_tmp.nuoviCasi);
				saveInCache(cacheVariazione, dateInterval, tot_tmp.nuoviCasi-var_tmp.nuoviCasi);
			}
			if(strcmp(elab.type, "T") == 0) {
				printf("%s: %i\n", dateInterval, tot_tmp.tamponi-var_tmp.tamponi);
				saveInCache(cacheVariazione, dateInterval, tot_tmp.nuoviCasi-var_tmp.nuoviCasi);
			}
			strcpy(filename_prec, filename_tmp);
		}

		strptime(filename_tmp, "%d:%m:%Y", &dateToConvert);
		nextDate = &dateToConvert;
		nextDate = nextDay(nextDate);
		dateToConvert = *nextDate;
		minDate = mktime(&dateToConvert);
		strftime(filename_tmp, sizeof(filename_tmp), "%d:%m:%Y", &dateToConvert);
		createFilePath(filepath_tmp, filename_tmp);
//printf("NUOVO FILENAME %s\n", filename_tmp);
	}
}

void floodingToDo() { 
	flooding = 0;
	while(difftime(minDate, maxDate) <= 0) {
		token = strtok(buffer_tmp, "-");	//taglio via RESPONSE-
		fd_tmp = fopen(filepath_tmp, "r");	
		if(fd_tmp == NULL) { //non ci sono file con quella data
printf("FILE NON PRESENTE\n");
			//controllo se il server ne ha
			token = strtok(NULL, "-");
			while(token != NULL)
			{
				sscanf(token, "%s %i %i", &DS_info.date, &DS_info.numPeerN, &DS_info.numPeerT);
//printf("DS_INFO %s %i %i\n", DS_info.str, DS_info.nuoviCasi, DS_info.tamponi);
//printf("FILENAME %s\n\n", filename_tmp);
				if(strcmp(filename_tmp, DS_info.date) == 0) { //data nei risultati del DS
					if((strcmp(elab.type, "N") == 0) && 
					(DS_info.numPeerN > 0))			//qualcuno ha registrato dati quel giorno
					{ 
printf("\tCASO N\n");
						flooding = 1;
						return;
					}
					if((strcmp(elab.type, "T") == 0) && 
					(DS_info.numPeerT > 0))
					{ 
printf("\tCASO T\n");
						flooding = 1;
						return;
					}
				}
				token = strtok(NULL, "-");
			}
		} else {	//il file è presente
printf("FILE PRESENTE\n");
			//leggo il totale dal file
			token = strtok(NULL, "-");
			while(token != NULL) {
				if(fscanf(fd_tmp, "%s %i\n",				//se il file è non vuoto
					&entry_tmp.type, &entry_tmp.quantity) != EOF) 
				{
					sscanf(token, "%s %i %i", &DS_info.date, 
							&DS_info.numPeerN, &DS_info.numPeerT);
					if(strcmp(filename_tmp, DS_info.date) == 0) {	//data in risultati DS 
						if((strcmp(elab.type, "N") == 0) && 
							(DS_info.numPeerN > 1))
						{ 
printf("CASO N > 1\n");
							fclose(fd_tmp);
							flooding = 1;
							return;
						}
					}
					if(strcmp(filename_tmp, DS_info.date) == 0) {
						if((strcmp(elab.type, "T") == 0) && 
						(DS_info.numPeerT > 1))
						{
printf("CASO T > 1\n");
							fclose(fd_tmp);
							flooding = 1;
							return;
						}
					}
				}
				token = strtok(NULL, "-");
			}
			fclose(fd_tmp);
		}
		
		strptime(filename_tmp, "%d:%m:%Y", &dateToConvert);
		nextDate = &dateToConvert;
		nextDate = nextDay(nextDate);
		dateToConvert = *nextDate;
		minDate = mktime(&dateToConvert);
		strftime(filename_tmp, sizeof(filename_tmp), "%d:%m:%Y", &dateToConvert);
		createFilePath(filepath_tmp, filename_tmp);
printf("NUOVO FILENAME %s\n", filename_tmp);
		//ricopio la lista del DS
		strcpy(buffer_tmp, buffer);
	}
}

void initCaseVariables() {
//percorso del file da aprire
	strcpy(filename_tmp, elab.p_date);

	//caso *,*
	if((strcmp(elab.p_date, "*") == 0) && (strcmp(elab.r_date, "*") == 0)) {
//printf("CASO * - *\n");
		//nome del file
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, inizio_pandemia);
		strcpy(filename_tmp, inizio_pandemia);

		//creo le variabili time_t per il confronto
		strptime(filename_tmp, "%d:%m:%Y", &dateToConvert);
		minDate = mktime(&dateToConvert);
		maxDate = mktime(todayDateTime);
		if(inTime() == 1)
			maxDate -= (60*60*24);					
	}

	//caso d:m:Y - *
	if((strcmp(elab.p_date, "*") != 0) && (strcmp(elab.r_date, "*") == 0)) {
//printf("CASO data - *\n");
		//nome del file
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, elab.p_date);
		strcpy(filename_tmp, elab.p_date);

		//creo le variaibli time_t per il confronto
		strptime(filename_tmp, "%d:%m:%Y", &dateToConvert);
		minDate = mktime(&dateToConvert);
		maxDate = mktime(todayDateTime);
		if(inTime() == 1)
			maxDate -= (60*60*24);
	}

	//caso * - d:m:Y
	if((strcmp(elab.p_date, "*") == 0) && (strcmp(elab.r_date, "*") != 0)) {
//printf("CASO * - data\n");
		//nome del file
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, inizio_pandemia);
		strcpy(filename_tmp, inizio_pandemia);

		//creo le variaibli time_t per il confronto
		strptime(filename_tmp, "%d:%m:%Y", &dateToConvert);
		minDate = mktime(&dateToConvert);
		strptime(elab.r_date, "%d:%m:%Y", &dateToConvert);
		maxDate = mktime(&dateToConvert);
	}

	//caso d:m:Y - d:m:Y
	if((strcmp(elab.p_date, "*") != 0) && (strcmp(elab.r_date, "*") != 0)) {
//printf("CASO data - data\n");
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, elab.p_date);
		strcpy(filename_tmp, elab.p_date);

		//creo le variaibli time_t per il confronto
		strptime(filename_tmp, "%d:%m:%Y", &dateToConvert);
		minDate = mktime(&dateToConvert);
		strptime(elab.r_date, "%d:%m:%Y", &dateToConvert);
		maxDate = mktime(&dateToConvert);
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
		strcpy(my_port, tmp_port);		
	}

	createRegisterName();
	strcpy(inizio_pandemia, "01:10:2020");
	strptime(inizio_pandemia, "%d:%m:%Y", &dateToConvert);
	beginningDate = mktime(&dateToConvert);

	while(1) {

		//reset dei descrittori
		FD_ZERO(&master);			//svuota master
		FD_ZERO(&read_fds);			//svuota read_fds

		FD_SET(0, &master);			//aggiunge stdin a master
		FD_SET(sd, &master);		//aggiunge sd a master

		//permette di controllare ogni minuto se sono le 18:00
		timeout = malloc(sizeof(timeout));
		timeout->tv_sec = 60;
		timeout->tv_usec = 0;

		read_fds = master;  
		select(fdmax+1, &read_fds, NULL, NULL, timeout);	
		// select ritorna quando un descrittore è pronto

		if(peer_connected == 1)
			checkTime();

		if (FD_ISSET(sd, &read_fds) && (peer_connected == 1)) {  //sd pronto in lettura

			receive_(srv_addr);

			howmany = parse_string(buffer);

			if(strcmp(command, "NEIGHBORS") == 0) {	//aggiornamento
			
				strcpy(my_neighbors.left_neighbor_ip, first_arg);
				strcpy(my_neighbors.left_neighbor_port, second_arg);
				strcpy(my_neighbors.right_neighbor_ip, third_arg);
				strcpy(my_neighbors.right_neighbor_port, fourth_arg);
				
				printf("My neighbors:\n\tleft:  { %s, %s }\n\tright: { %s, %s }\n", 
					   my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port, 
					   my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);		
			}

			if(strcmp(command, "ACK") == 0) {
				printf("Dati odierni correttamente inviati");
			}

			if(strcmp(command, "ESC") == 0) {
				printf("Chiusura a causa della terminazione del DS\n");
				closing_actions();
				exit(0);
			}

			if(strcmp(command, "REQ_DATA") == 0) {
				//se ho il dato in cache lo invio
				//if() {
					printf("DATO IN CACHE\n");
					//metto i dati nel buffer
					//printf("REPLY DATA %s", buffer);
					//send_(addr);
				//} else {
					//printf("REPLY DATA");
					//send_(addr);
				//}
			}

			if(strcmp(command, "REPLY_DATA") == 0) {
				token = strtok(buffer, " ");
				token = strtok(buffer, " ");
				if(token != NULL) {
					//prelevo dato
					printf("prelevo dato\n");
					//elaboro dati
					//stampo dati
				} else {
					if(strcmp(side, "L") == 0) {
						connect_to_peer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
						printf("FLOOD_FOR_ENTRIES L %s %s %s %s", 
								elab.aggr, elab.type, elab.p_date, elab.r_date);
					}
					if(strcmp(side, "R") == 0) {
						connect_to_peer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
						printf("FLOOD_FOR_ENTRIES R %s %s %s %s", 
								elab.aggr, elab.type, elab.p_date, elab.r_date);
					}
					send_(addr);	
				}
			}

			if(strcmp(command, "FLOOD_FOR_ENTRIES") == 0) {
				if(strcmp(first_arg, side) != 0) {
					if((strcmp(first_arg, "L") == 0) && (strcmp(side, "R"))) {	//risposta
						connect_to_peer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
						sprintf(buffer, "REPLY_FLOOD R %s", my_port);
					} 
					if((strcmp(first_arg, "R") == 0) && (strcmp(side, "L"))) {
						connect_to_peer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
						sprintf(buffer, "REPLY_FLOOD L %s", my_port);
					}
				} else {
					if(strcmp(side, "L") == 0) 	//inoltro
						connect_to_peer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
					if(strcmp(side, "R") == 0) 	//inoltro
						connect_to_peer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
				}
				send_(addr);
			}

			if(strcmp(command, "REPLY_FLOOD") == 0) {
				if(strcmp(side, "L") == 0) 	
					connect_to_peer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
				if(strcmp(side, "R") == 0)	
					connect_to_peer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
				sprintf(buffer+strlen(buffer), "%s", my_port);
				send_(addr);
			}
/*
			if(strcmp(command, "REQ_ENTRIES") == 0) {


			}

			if(strcmp(command, "REPLY_ENTRIES") == 0) {

			}
*/
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
				connect_to_DS(first_arg, second_arg);

				num_open_fds = nfds = 2;	
				pfds = calloc(nfds, sizeof(struct pollfd));
				if(pfds == NULL)
					perror("Allocazione pfds fallita");
				pfds[0].fd = 0;
				pfds[0].events = POLLIN;
				pfds[1].fd = sd;
				pfds[1].events = POLLIN;

				while(1) {
					
					sprintf(buffer, "BOOT %s %s", localhost, my_port);
					send_(srv_addr);

					ready = poll(pfds, nfds, 4000);

					if(ready) {
						if(pfds[1].revents) {	// riceve qualcosa da DS
							
							receive_(srv_addr);
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
				
				createRegisterName();
				recoverPreviousData();

				fd = fopen(filepath, "a");
				if(fd == NULL)
					perror("Error: ");
				else 
					printf("File open\n");
			}


			if((strcmp(command, "add") == 0) && (valid_input == 1)) {
				if(peer_connected == 0)
					printf("Errore: peer non connesso al DS\n");
				else if((strcmp(first_arg, "N") != 0) && (strcmp(first_arg, "T") != 0))
					printf("Formato invalido, digitare: add [N|T] quantità\n");
				else {				
					if(strcmp(first_arg, "N") == 0) {
						tot.nuoviCasi = tot.nuoviCasi+atoi(second_arg);
					}
					if (strcmp(first_arg, "T") == 0) {
						tot.tamponi = tot.tamponi+atoi(second_arg);
					}
					printf("TOTALE: tamponi: %i nuovi casi: %i\n",
						tot.tamponi, tot.nuoviCasi);
					//salvataggio su file
					updateRegister();
				}
			}	

			if((strcmp(command, "get") == 0) && (valid_input == 1)) {
				valid_period = parse_period(third_arg);
				if((strcmp(first_arg, "totale") != 0 && strcmp(first_arg, "variazione") != 0) ||
				    (strcmp(second_arg, "N") != 0) && (strcmp(second_arg, "T") != 0) ||
					(valid_period == 0)) 
				{					
					printf("Formato invalido, digitare: ");
					printf("get [totale|variazione] [N|T] dd1:mm1:yyyy1-dd2:mm2:yyyy2\n");

				} else {
					//compilo struct Aggr
					strcpy(elab.aggr, first_arg);
					strcpy(elab.type, second_arg);
				
					sprintf(buffer, "GET %s %s %s", elab.type, elab.p_date, elab.r_date);
					send_(srv_addr);

					receive_(srv_addr);
					strcpy(buffer_tmp, buffer);

					initCaseVariables();
					floodingToDo();
	
printf("FLOODING: %i \n", flooding);
					if(flooding == 0) {
						initCaseVariables();
						//calcolo dato
						if(strcmp(elab.aggr, "totale") == 0) 
							getLocalTotal();
						if(strcmp(elab.aggr, "variazione") == 0) 
							localVariation();
						//memorizzare dato
					}
					else {
						connect_to_peer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
						sprintf(buffer, "REQ_DATA %s %s %s %s", elab.aggr, elab.type, elab.p_date, elab.r_date);
						send_(addr);
						
						connect_to_peer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
						sprintf(buffer, "REQ_DATA %s %s %s %s", elab.aggr, elab.type, elab.p_date, elab.r_date);
						send_(addr);					
					}					
				}
			}	

			if((strcmp(command, "stop") == 0) && (valid_input == 1)) {	
			
				if(peer_connected) {
					sprintf(buffer, "QUIT %s %s", localhost, my_port);
					send_(srv_addr);
				}

				printf("Terminazione forzata\n");
				closing_actions();
				exit(0);
			}
		}	
	} //while
	closing_actions();
} //main


