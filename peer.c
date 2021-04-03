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

int ret, sd, len, i;
char localhost[ADDR_LEN] = "127.0.0.1\0";
char my_port[PORT_LEN];	//porta del peer attuale
char* tmp_port;				//variabile per prelevare porta da terminale
char peer_port[ADDR_LEN];

int peer_connected = 0;		//indica se il socket è stato creato
int peer_registered = 0;	//indica se il peer è registrato con un DS	

//variabili per prelevare i campi di un messaggio
char input[BUFFER_LEN];
char *token;				//per l'utilizzo di strtok
int valid_input = 1;		//indica se un comando ha il fomato corretto
int how_many;				//numero token (compreso cmd)
char command[CMD_LEN+1];	//primo campo di un messaggio
char first_arg[BUFFER_LEN];
char second_arg[BUFFER_LEN];
char third_arg[BUFFER_LEN];
char fourth_arg[BUFFER_LEN];
char fifth_arg[BUFFER_LEN];
char sixth_arg[BUFFER_LEN];

//variabili per prelevare i campi del periodo
int valid_period = 1;
struct Request {
	int req_id;
	char aggr[DATE_LEN];
	char type[2];
	char p_date[DATE_LEN];
	char r_date[DATE_LEN];
} elab, peer_req;
char side[2];
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

struct DS_Results {
	char date[DATE_LEN];
	int numEntryN;
	int numEntryT;
} DS_info;

struct Totale {
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
int flooding, found, close_neig_response;
struct tm *nextDate;
char filename_prec[DATE_LEN];
char dateInterval[DATE_LEN+DATE_LEN];

struct Cache {
	char aggr[BUFFER_LEN];
	char type[BUFFER_LEN];
	char p_date[BUFFER_LEN];
	char r_date[BUFFER_LEN];
	char date[BUFFER_LEN];
	int result;
} cache_entry;

struct Result {
	char date[BUFFER_LEN];
	int result;
} res;       

struct NeighborEntry {
	char date[DATE_LEN];
	int result;
} neigh_entry;

//variabili flooding
char buffer_tmp[BUFFER_LEN];
char cacheTotale[] = "totale.txt";
char cacheVariazione[] = "variazione.txt";
struct DataToSend {
	int id;
	int num_entries;
	char aggr[BUFFER_LEN];
	char type[BUFFER_LEN];
	char p_date[BUFFER_LEN];
	char r_date[BUFFER_LEN];
	char variation[BUFFER_LEN];
	int total;
	struct DataToSend *next;
};
struct StoredResults
{
	int num;
	struct DataToSend *list;
} store;
struct DataToSend *p;
int flooded = 0, q, t, got_data;
int counter_left, counter_right, counter, tot_parziale;
int entries_to_gain, entry_gained, num_entries, period_entries;
char di[DATE_LEN+DATE_LEN], variation[BUFFER_LEN], peer_to_contact[BUFFER_LEN], var_parziale[BUFFER_LEN];

void closingActions() {	//azioni da compiere quando un peer termina
	free(tmp_port);
	free(token);
	free(timeout);

	if(peer_connected == 1) {
		close(sd);
		//fclose(fd);
		printf("Chiusura del socket effettuata\n");
	}
	FD_CLR(0, &master);
	FD_CLR(sd, &master);
}

void connectToPeer(char peer_addr[], char peer_port[]) {
    /* Creazione indirizzo del peer da cui ricevere */
    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(peer_port));
    inet_pton(AF_INET, peer_addr, &addr.sin_addr);
}

void connectToDS(char DS_addr[], char DS_port[]) {		//creazione del socket
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
		printf("Invio: [%s] a %d\n", buffer, ntohs(a.sin_port));       
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
	printf("Ricevuto: [%s] da %d\n", buffer, ntohs(s.sin_port));
}

int parseString(char buffer[]) {	//separa gli argomenti di un comando

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
			case 5:
				sscanf(token, "%s", &fifth_arg);
				break;
			case 6:
				sscanf(token, "%s", &sixth_arg);
				break;
			default:
//				printf("Comando non riconosciuto\n");
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

int parsePeriod(char buffer[]) {	//separa le date del periodo

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

	strptime(elab.p_date, "%d_%m_%Y", &dateToConvert);
	past_date = mktime(&dateToConvert);
	strptime(elab.r_date, "%d_%m_%Y", &dateToConvert);
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
			if(strcmp(my_entry.type, "N") == 0)
				tot.nuoviCasi ++;
			if(strcmp(my_entry.type, "T") == 0)
				tot.tamponi ++;
		}
		printf("Recovered: %i %i\n", tot.tamponi, tot.nuoviCasi);
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

void communicateToDS() {
	sprintf(buffer, "TODAY_ENTRIES %i %i", tot.nuoviCasi, tot.tamponi);
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
//printf("MONTHDAYS: %i\n", monthDays);
//printf("today  ->  %i:%i:%i\n", today->tm_mday, today->tm_mon, today->tm_year);
//printf("dd mm YY  ->  %i:%i:%i\n", atoi(dd), atoi(mm), atoi(YY));
//printf("tempDate  ->  %i:%i:%i\n", tmpDate->tm_mday, tmpDate->tm_mon, tmpDate->tm_year);

	if(today->tm_mday != monthDays) {
//printf("CASO 1\n");
		tmpDate->tm_mday = atoi(dd)+1;	
		tmpDate->tm_mon = atoi(mm)-1;  
		tmpDate->tm_year = atoi(YY)-1900;  
	}
	if(today->tm_mday == monthDays) {
//printf("CASO 2\n");
		tmpDate->tm_mday = 1;
		tmpDate->tm_mon = atoi(mm);
		tmpDate->tm_year = atoi(YY)-1900;
	}
	if((today->tm_mon == 11) && (today->tm_mday == 31)) {
//printf("CASO 3\n");
		tmpDate->tm_mday = 1;
		tmpDate->tm_mon = 0;
		tmpDate->tm_year = atoi(YY)+1-1900;
	}
//printf("dd:mm:YY  ->  %i:%i:%i\n", tmpDate->tm_mday, tmpDate->tm_mon, tmpDate->tm_year);
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
		strftime(filename, sizeof(filename), "%d_%m_%Y", tomorrowDateTime);
	} else {
		strftime(filename, sizeof(filename), "%d_%m_%Y", todayDateTime);
	}
	printf("File open: %s\n", filename);
	createFilePath(filepath, filename);
}

void checkTime() {	//controlla se bisogna chiudere il file
	
	time(&now);
	todayDateTime = localtime(&now);
	strftime(timeToCheck, sizeof(filename), "%R", todayDateTime);

	if(strcmp(timeToCheck, "18:00\0") == 0) {
		printf("Time's over: %s\n", timeToCheck);
		fclose(fd);
		communicateToDS();
		printf("Registro della data odierno chiuso\n");
		
		tomorrowDateTime = nextDay(todayDateTime); 
		strftime(filename, sizeof(filename), "%d_%m_%Y", tomorrowDateTime);

//printf("FILENAME: %s\n", filename);

		createFilePath(filepath, filename);		
		//apre file giorno successivo
		fd = fopen(filepath, "a");
		if(fd == NULL)
			perror("Error: ");
	}
}

void saveInCache (char cache[], char date[], int quantity) {
	createFilePath(filepath, cache);
	fd_tmp = fopen(filepath, "a");
	if(fd_tmp) {
		if(strcmp(cache, cacheTotale) == 0)
			fprintf(fd_tmp, "%s %s %s %s %i\n", elab.aggr, elab.type, elab.p_date,
					elab.r_date, quantity);
		if(strcmp(cache, cacheVariazione) == 0)
			fprintf(fd_tmp, "%s %s %s %s %s %i\n", elab.aggr, elab.type, elab.p_date,
					elab.r_date, date, quantity);
		fclose(fd_tmp);
	}
}

int searchInCache(char cache[], int caso) {	
	printf("Ricerca in ");
	createFilePath(filepath, cache);
	fd_tmp = fopen(filepath, "r");
	if(fd_tmp) {
		if(strcmp(cache, cacheTotale) == 0) {
			printf("totale.txt\n");
			switch(caso) {
				case 0:	//cerca e se trova stampa a video
					while(fscanf(fd_tmp, "%s %s %s %s %i\n", &cache_entry.aggr, 
							&cache_entry.type, &cache_entry.p_date, 
							&cache_entry.r_date, &cache_entry.result) != EOF) 
					{
//printf("CACHE: %s %s %s %s %i\n", cache_entry.aggr, 
//							cache_entry.type, cache_entry.p_date, 
//							cache_entry.r_date, cache_entry.result);
						if((strcmp(cache_entry.aggr, elab.aggr) == 0) &&
							(strcmp(cache_entry.type, elab.type) == 0) &&
							(strcmp(cache_entry.p_date, elab.p_date) == 0) &&
							(strcmp(cache_entry.r_date, elab.r_date) == 0)) 
						{
							//stampa a video
							printf("TOTALE: %i\n", cache_entry.result);
							fclose(fd_tmp);
							return 1;
						}
					}
					break;
				case 1:	//cerca e se trova scrive nel buffer
					while(fscanf(fd_tmp, "%s %s %s %s %i\n", &cache_entry.aggr, 
							&cache_entry.type, &cache_entry.p_date, 
							&cache_entry.r_date, &cache_entry.result) != EOF) 
					{
						if((strcmp(cache_entry.aggr, peer_req.aggr) == 0) &&
							(strcmp(cache_entry.type, peer_req.type) == 0) &&
							(strcmp(cache_entry.p_date, peer_req.p_date) == 0) &&
							(strcmp(cache_entry.r_date, peer_req.r_date) == 0)) 
						{
							//scrive risultato nel buffer per REPLY_DATA
							sprintf(buffer + strlen(buffer), "%i", cache_entry.result);
							fclose(fd_tmp);
							return 1;
						}
					}
					break;
				default:
					break;
			}
		}
		if(strcmp(cache, cacheVariazione) == 0) {
			printf("variazione.txt\n");
			found = 0;
			switch(caso) {
				case 0:
					while(fscanf(fd_tmp, "%s %s %s %s %s %i\n", &cache_entry.aggr,
					&cache_entry.type, &cache_entry.p_date, &cache_entry.r_date,
					&cache_entry.date, &cache_entry.result) != EOF) 
					{
						if((strcmp(cache_entry.aggr, elab.aggr) == 0) &&
							(strcmp(cache_entry.type, elab.type) == 0) &&
							(strcmp(cache_entry.p_date, elab.p_date) == 0) &&
							(strcmp(cache_entry.r_date, elab.r_date) == 0)) 
						{
							//stampa a video
							found = 1;
							printf("Variazione: %s %i\n", cache_entry.date, cache_entry.result);
						}
					}
					if(found == 1) {
						fclose(fd_tmp);
						return 1;
					}	
					break;
				case 1:
					while(fscanf(fd_tmp, "%s %s %s %s %s %i\n", &cache_entry.aggr,
					&cache_entry.type, &cache_entry.p_date, &cache_entry.r_date,
					&cache_entry.date, &cache_entry.result) != EOF) 
					{
						if((strcmp(cache_entry.aggr, peer_req.aggr) == 0) &&
							(strcmp(cache_entry.type, peer_req.type) == 0) &&
							(strcmp(cache_entry.p_date, peer_req.p_date) == 0) &&
							(strcmp(cache_entry.r_date, peer_req.r_date) == 0)) 
						{
							//scrive risultato nel buffer per REPLY_DATA
							sprintf(buffer+strlen(buffer), "%s %i/", cache_entry.date, cache_entry.result);
							found = 1;
						}
					}
					if(found == 1) {
						fclose(fd_tmp);
						return 1;
					}	
					break;
				deafult:
					break;
			}
		}
		fclose(fd_tmp);
	}
	return 0;
}

void getLocalTotal(int caso, char type[]) { 
//printf("GET LOCAL TOTAL \n");
	num_entries = tot_tmp.nuoviCasi = tot_tmp.tamponi = 0;
	while(difftime(minDate, maxDate) <= 0) {
//printf("MinDate: %s MaxDate: %s\n", ctime(&minDate), ctime(&maxDate));
		fd_tmp = fopen(filepath_tmp, "r");
		if(fd_tmp != NULL) {
			while(fscanf(fd_tmp, "%s %i\n",				//per tutti i dati del file
					&entry_tmp.type, &entry_tmp.quantity) != EOF) 
			{
//printf("ENTRY: %s %i\n", entry_tmp.type, entry_tmp.quantity);
				if((strcmp(entry_tmp.type, "N") == 0) && (strcmp(type, "N") == 0)) {
					tot_tmp.nuoviCasi += entry_tmp.quantity;
					num_entries++;
				}
				if((strcmp(entry_tmp.type, "T") == 0) && (strcmp(type, "T") == 0)){
					tot_tmp.tamponi += entry_tmp.quantity;
					num_entries++;
				}
//printf("NUM ENTRY: %i\n", num_entries);
			}
			fclose(fd_tmp);
		}
//printf("TOT: %i %i\n", tot_tmp.nuoviCasi, tot_tmp.tamponi);
		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
		nextDate = &dateToConvert;
		nextDate = nextDay(nextDate);
		dateToConvert = *nextDate;
		minDate = mktime(&dateToConvert)+TZ;
//printf("LT MIN DATE: %s\n", ctime(&minDate));
		strftime(filename_tmp, sizeof(filename_tmp), "%d_%m_%Y", nextDate);
		//aggiorno filepath
		createFilePath(filepath_tmp, filename_tmp);
//printf("NUOVO FILENAME %s\n", filename_tmp);
	}
	if(caso == 0){		//stampa a video il risultato
		printf("TOTALE: ");
		if(strcmp(type, "N") == 0) {
			printf("%i\n", tot_tmp.nuoviCasi);
			if(strcmp(elab.r_date, "*") != 0)
				saveInCache(cacheTotale, "", tot_tmp.nuoviCasi);
		}
		if(strcmp(type, "T") == 0) {
			printf("%i\n", tot_tmp.tamponi);
			if(strcmp(elab.r_date, "*") != 0)
				saveInCache(cacheTotale, "", tot_tmp.tamponi);
		}	
	}
	if(caso == 1) { 	//precalcolo per dopo
//printf("CASO 1 DI getTotal\n");
		if((tot_tmp.nuoviCasi > 0) || (tot_tmp.tamponi > 0)) {
			p = store.list;
			if(p == NULL) {
//printf("M_ALLOC IN TESTA\n");
				p = (struct DataToSend *)malloc(sizeof(struct DataToSend)*10);
			} else {
				while(p->next != NULL) 
					p = p->next;
				p->next = (struct DataToSend *)malloc(sizeof(struct DataToSend));
				p = p->next;
			}
			store.num++;
			p->id = peer_req.req_id; 
			p->num_entries = num_entries;
			strcpy(p->aggr, peer_req.aggr);
			strcpy(p->type, peer_req.type);
			strcpy(p->p_date, peer_req.p_date);
			strcpy(p->r_date, peer_req.r_date);
			p->next = NULL;
			if(!store.list)
				store.list = p;
//printf("STORE: %i, %s, %s, %s %s %i\n", p->id, p->aggr, p->type, p->p_date, p->r_date, p->total);
		}
		if((strcmp(type, "N") == 0) && (tot_tmp.nuoviCasi > 0)) 
			p->total = tot_tmp.nuoviCasi;
		if((strcmp(type, "T") == 0) && (tot_tmp.tamponi > 0))
			p->total = tot_tmp.tamponi;
	}
	if(caso == 2) {
//printf("Tot_tmp.nuoviCasi = %i\n", tot_tmp.nuoviCasi);
		printf("TOTALE: ");
		if(strcmp(type, "N") == 0) {
			tot_parziale += tot_tmp.nuoviCasi;
			printf("%i\n", tot_parziale);
		}
		if(strcmp(type, "T") == 0) {
			tot_parziale += tot_tmp.tamponi;
			printf("%i\n", tot_parziale);
		}
		if((strcmp(elab.r_date, "*") != 0) && (entry_gained == entries_to_gain))
			saveInCache(cacheTotale, "", tot_parziale);
	}
}

void getLocalVariation(int caso, char type[]){   
//printf("GET LOCAL VARIATION\n");
	strcpy(filename_prec, filename_tmp);
	sprintf(variation, "");
	tot_tmp.nuoviCasi = tot_tmp.tamponi = num_entries = 0;
	while(difftime(minDate, maxDate) <= 0) {
		fd_tmp = fopen(filepath_tmp, "r");
		tot_tmp.nuoviCasi = var_tmp.nuoviCasi;	
		tot_tmp.tamponi = var_tmp.tamponi; 
		var_tmp.nuoviCasi = var_tmp.tamponi = 0;
		if(fd_tmp != NULL) {
			fseek(fd_tmp, 0, SEEK_END);
			if(ftell(fd_tmp) > 0) {
				fseek(fd_tmp, 0, SEEK_SET);
				while(fscanf(fd_tmp, "%s %i\n",				//per tutti i dati del file
						&entry_tmp.type, &entry_tmp.quantity) != EOF)
				{
//printf("ENTRY: %s %i\n", entry_tmp.type, entry_tmp.quantity);
					if((strcmp(entry_tmp.type, "N") == 0) && (strcmp(type, "N") == 0)) {
						var_tmp.nuoviCasi += entry_tmp.quantity;
						num_entries++;
					}
					if((strcmp(entry_tmp.type, "T") == 0) && (strcmp(type, "T")) == 0) {
						var_tmp.tamponi += entry_tmp.quantity;
						num_entries++;
					}
				}
			} else {
				var_tmp.nuoviCasi = 0;
				var_tmp.tamponi = 0;
			}
			fclose(fd_tmp);
		}
		else { 
			var_tmp.nuoviCasi = 0;
			var_tmp.tamponi = 0;
		}
//printf("VAR N %i VAR T %i\n", var_tmp.nuoviCasi, var_tmp.tamponi);
//printf("TOT N %i TOT T %i\n", tot_tmp.nuoviCasi, tot_tmp.tamponi);
		if(strcmp(filename_prec, filename_tmp) != 0){
			sprintf(dateInterval, "%s-%s", filename_prec, filename_tmp);
			if(caso  == 0) {
				if(strcmp(type, "N") == 0) {
					i = tot_tmp.nuoviCasi-var_tmp.nuoviCasi;
					printf("Variazione %s: %i\n", dateInterval, i);
					if(strcmp(elab.r_date, "*") != 0)
						saveInCache(cacheVariazione, dateInterval, i);
				}
				if(strcmp(type, "T") == 0) {
					i = tot_tmp.tamponi-var_tmp.tamponi;
					printf("Variazione %s: %i\n", dateInterval, i);
					if(strcmp(elab.r_date, "*") != 0)
						saveInCache(cacheVariazione, dateInterval, i);
				}
			}
			if(caso == 1) {
				if(strcmp(type, "N") == 0) {
					i = tot_tmp.nuoviCasi-var_tmp.nuoviCasi;
					if(i != 0)
						sprintf(variation+strlen(variation), "%s %i/", dateInterval, i);
				}
				if(strcmp(type, "T") == 0) {
					i = tot_tmp.tamponi-var_tmp.tamponi;
					if(i != 0)
						sprintf(variation+strlen(variation), "%s %i/", dateInterval, i);
				}				
			}
			if(caso == 2) {
				strcpy(buffer_tmp, var_parziale);
				token = strtok(buffer_tmp, "/");
				q = 0;
				while (token != NULL) {  //preleva dato [data quantità]
					sscanf(token, "%s %i", &di, &t);
					if(strcmp(di, dateInterval) == 0) {	//data presente in REPLY_ENTRIES message
						q += t;
					} 
					token = strtok(NULL, "/");
				}
				if(strcmp(type, "N") == 0) {
					i = tot_tmp.nuoviCasi-var_tmp.nuoviCasi+q;
					printf("Variazione %s: %i\n", dateInterval, i);
				}
				if(strcmp(type, "T") == 0) {
					i = tot_tmp.tamponi-var_tmp.tamponi+q;
					printf("Variazione %s: %i\n", dateInterval, i);
				}
				if((strcmp(elab.r_date, "*") != 0) && (entry_gained == entries_to_gain))
					saveInCache(cacheVariazione, dateInterval, i);
			}
			strcpy(filename_prec, filename_tmp);
		}

		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
		nextDate = &dateToConvert;
		nextDate = nextDay(nextDate);
		dateToConvert = *nextDate;
		minDate = mktime(&dateToConvert)+TZ;
//printf("LV MIN DATE: %s\n", ctime(&minDate));
		strftime(filename_tmp, sizeof(filename_tmp), "%d_%m_%Y", nextDate);
		createFilePath(filepath_tmp, filename_tmp);
//printf("NUOVO FILENAME %s\n", filename_tmp);
	}
	if((caso == 1) && (strlen(variation) > 0)) {
		p = store.list;
		if(p == NULL) {
			p = (struct DataToSend *)malloc(sizeof(struct DataToSend));
			printf("ALLOCO IN TESTA\n");
		}
		else {
			while(p->next)
				p = p->next;
			p->next = (struct DataToSend *)malloc(sizeof(struct DataToSend));
			p = p->next;
			printf("ALLOCO IN CODA\n");
		}
		store.num++;
		p->id = peer_req.req_id;
		p->num_entries = num_entries;
		strcpy(p->aggr, peer_req.aggr);
		strcpy(p->type, peer_req.type);
		strcpy(p->p_date, peer_req.p_date);
		strcpy(p->r_date, peer_req.r_date);
		strcpy(p->variation, variation);
		p->next = NULL;
//printf("STORE: %i, %s, %s, %s %s %i\n", p->id, p->aggr, p->type, p->p_date, p->r_date, p->variation);
		if(!store.list)
			store.list = p;
	}
}

void initCaseVariables(char dateP[], char dateR[]) {
//printf("INIT CASE VARIABLES\n");
	//percorso del file da aprire
	strcpy(filename_tmp, dateP);
//printf("DATETOCONVERT: %i %i %i\n", dateToConvert.tm_hour, dateToConvert.tm_min, dateToConvert.tm_sec);
dateToConvert.tm_hour = dateToConvert.tm_min = dateToConvert.tm_sec = 0;

	//caso *,*
	if((strcmp(dateP, "*") == 0) && (strcmp(dateR, "*") == 0)) {
//printf("CASO * - *\n");
		//nome del file
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, inizio_pandemia);
		strcpy(filename_tmp, inizio_pandemia);

		//creo le variabili time_t per il confronto
		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
		minDate = mktime(&dateToConvert);
		maxDate = mktime(todayDateTime);
		if(inTime() == 1)
			maxDate -= (60*60*24);					
	}

	//caso d:m:Y - *
	if((strcmp(dateP, "*") != 0) && (strcmp(dateR, "*") == 0)) {
//printf("CASO data - *\n");
		//nome del file
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, dateP);
		strcpy(filename_tmp, dateP);

		//creo le variaibli time_t per il confronto
		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
		minDate = mktime(&dateToConvert);
		maxDate = mktime(todayDateTime);
		if(inTime() == 1)
			maxDate -= (60*60*24);
	}

	//caso * - d:m:Y
	if((strcmp(dateP, "*") == 0) && (strcmp(dateR, "*") != 0)) {
//printf("CASO * - data\n");
		//nome del file
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, inizio_pandemia);
		strcpy(filename_tmp, inizio_pandemia);

		//creo le variaibli time_t per il confronto
		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
		minDate = mktime(&dateToConvert);
		strptime(dateR, "%d_%m_%Y", &dateToConvert);
		maxDate = mktime(&dateToConvert);
	}

	//caso d:m:Y - d:m:Y
	if((strcmp(dateP, "*") != 0) && (strcmp(dateR, "*") != 0)) {
//printf("CASO data - data\n");
//printf("FILE TO OPEN: %s\n", filename_tmp);
		createFilePath(filepath_tmp, dateP);
		strcpy(filename_tmp, dateP);

		//creo le variaibli time_t per il confronto
		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
//printf("DATETOCONVERT: %i %i %i\n", dateToConvert.tm_hour, dateToConvert.tm_min, dateToConvert.tm_sec);
		minDate = mktime(&dateToConvert);
		strptime(dateR, "%d_%m_%Y", &dateToConvert);
		maxDate = mktime(&dateToConvert);
	}
//	printf("MIN DATE: %s\n", ctime(&minDate));
//	printf("MAX DATE: %s\n", ctime(&maxDate));
}


void getPeriodEntries() {	//entries totali inserite nel periodo
	while(difftime(minDate, maxDate) <= 0) {
		token = strtok(buffer_tmp, "-");
		token = strtok(NULL, "-");
		while(token != NULL)		//finché c'è una entry nel RESPONSE msg
		{
			sscanf(token, "%s %i %i", &DS_info.date, &DS_info.numEntryN, &DS_info.numEntryT);	//legge la DS entry
//printf("DS_INFO %s %i %i\n", DS_info.date, DS_info.numEntryN, DS_info.numEntryT);
			if(strcmp(filename_tmp, DS_info.date) == 0) { //data nei risultati del DS
				if((strcmp(elab.type, "N") == 0) && (DS_info.numEntryN > 0))			//qualcuno ha registrato dati quel giorno
					period_entries += DS_info.numEntryN;
				if((strcmp(elab.type, "T") == 0) && (DS_info.numEntryT > 0))
					period_entries += DS_info.numEntryT;
			}
			token = strtok(NULL, "-");
		}
		//genera prossima data del periodo
		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
		nextDate = &dateToConvert;
//printf("FILENAME %s\n", filename_tmp);
		nextDate = nextDay(nextDate);
		dateToConvert = *nextDate;
		minDate = mktime(&dateToConvert)+TZ;
//printf("FTD MIN DATE: %s\n", ctime(&minDate));
		strftime(filename_tmp, sizeof(filename_tmp), "%d_%m_%Y", nextDate);
		createFilePath(filepath_tmp, filename_tmp);
//printf("NUOVO FILENAME %s\n\n", filename_tmp);
		//ricopio la lista del DS
		strcpy(buffer_tmp, buffer);
	}
}

int getLocalEntries() {		//entries locali del periodo
	while(difftime(minDate, maxDate) <= 0) {
		fd_tmp = fopen(filepath_tmp, "r");
		if(fd_tmp != NULL) {	//se il èfile non presente
			fseek(fd_tmp, 0, SEEK_END);
			if(ftell(fd_tmp) > 0)		//se il file è non vuoto
			{
				fseek(fd_tmp, 0, SEEK_SET);				
				while(fscanf(fd_tmp, "%s %i\n", &my_entry.type, &my_entry.quantity) != EOF) {
					if(strcmp(my_entry.type, elab.type) == 0)
						num_entries++;
				}					
			}
		} 
		//genera prossima data del periodo
		strptime(filename_tmp, "%d_%m_%Y", &dateToConvert);
		nextDate = &dateToConvert;
//printf("FILENAME %s\n", filename_tmp);
		nextDate = nextDay(nextDate);
		dateToConvert = *nextDate;
		minDate = mktime(&dateToConvert)+TZ;
//printf("FTD MIN DATE: %s\n", ctime(&minDate));
		strftime(filename_tmp, sizeof(filename_tmp), "%d_%m_%Y", nextDate);
		createFilePath(filepath_tmp, filename_tmp);
//printf("NUOVO FILENAME %s\n\n", filename_tmp);
		//ripristina la lista del DS
		strcpy(buffer_tmp, buffer);
	}
}

void floodingToDo() { 
//printf("FLOODING TO DO\n");
	flooding = entries_to_gain = num_entries = period_entries = 0;
	getLocalEntries();	//inizializza num_entries
//printf("NUM ENTRY: %i\n", num_entries);
	initCaseVariables(elab.p_date, elab.r_date);
	getPeriodEntries();	//inizializza period_entries
//printf("PERIOD ENTRIES: %i\n", period_entries);
	entries_to_gain = period_entries - num_entries;
//printf("ENTRIES_TO_GAIN: %i\n", entries_to_gain);
	if (entries_to_gain == 0)
		flooding = 0;
	else
		flooding = 1;
}

int main(int argc, char* argv[]){

	tmp_port = (char*)malloc(sizeof(char)*ADDR_LEN);
	token = (char*)malloc(sizeof(char)*BUFFER_LEN);
	p = (struct DataToSend *)malloc(sizeof(struct DataToSend));
	store.num = 0;
	store.list = NULL;
	if((tmp_port == NULL) || (token == NULL)){
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

	srand(time(NULL));		//genera identificativo per la richiesta
	createRegisterName();
	strcpy(inizio_pandemia, "01_03_2021");
//	strptime(inizio_pandemia, "%d:%m:%Y", &dateToConvert);
		strptime(inizio_pandemia, "%d_%m_%Y", &dateToConvert);
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
			strcpy(input, buffer);
			how_many = parseString(buffer);

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
				closingActions();
				exit(0);
			}

			if(strcmp(command, "REQ_DATA") == 0) {
				strcpy(peer_req.aggr, second_arg);
				strcpy(peer_req.type, third_arg);
				strcpy(peer_req.p_date, fourth_arg);
				strcpy(peer_req.r_date, fifth_arg);
				got_data = 0;
				//se ho il dato in cache lo invio
				sprintf(buffer, "REPLY_DATA ");
				if((strcmp(peer_req.aggr, "totale") == 0) && (strcmp(peer_req.r_date, "*") != 0))
					searchInCache(cacheTotale, 1);
				if((strcmp(peer_req.aggr, "variazione") == 0) && (strcmp(peer_req.r_date, "*") != 0))
					searchInCache(cacheVariazione, 1);
				connectToPeer(localhost, first_arg);
				send_(addr);
			}

			if(strcmp(command, "REPLY_DATA") == 0) {
				close_neig_response++;		//quando è == 2 entrambi i vicini hanno risposto
//printf("REPLY DATA CLOSE_NEIGH: %i\n", close_neig_response);
				strcpy(buffer, input);
				token = strtok(buffer, " ");	//token = "REPLY_DATA"
				if(strcmp(elab.aggr, "totale") == 0) 
					token = strtok(NULL, " ");
				if(strcmp(elab.aggr, "variazione") == 0)
					token = strtok(NULL, "/");
				if((token != NULL) && (got_data == 0)) {	//il vicino aveva il risultato
printf("Il vicino ha i dati\n");
					got_data = 1;
					//stampo dati
					if(strcmp(elab.aggr, "totale") == 0) {
						sscanf(token, "%i", &res.result);
						saveInCache(cacheTotale, "", res.result);
						printf("Totale: %i\n", res.result);
					}
					if(strcmp(elab.aggr, "variazione") == 0) {
						while(token != NULL) {
							sscanf(token, "%s %i", &res.date, &res.result);
							printf("Variazione %s: %i\n", res.date, res.result);
							saveInCache(cacheVariazione, res.date, res.result);
							token = strtok(NULL, "/");
						}
					}
				} else {
printf("Il vicino non ha i dati\n");
					if(got_data == 0) {
						//se ha un vicino solo
						if((strcmp(my_neighbors.left_neighbor_ip, "-") == 0) || (strcmp(my_neighbors.right_neighbor_ip, "-") == 0)) {
							if((strcmp(my_neighbors.left_neighbor_ip, "-") != 0) && (strcmp(my_neighbors.right_neighbor_ip, "-") == 0)) {
								connectToPeer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
								sprintf(buffer, "FLOOD_FOR_ENTRIES %i L %s %s %s %s", elab.req_id,
										elab.aggr, elab.type, elab.p_date, elab.r_date);
								send_(addr);
							}
							if((strcmp(my_neighbors.left_neighbor_ip, "-") == 0) && (strcmp(my_neighbors.right_neighbor_ip, "-") != 0)) {
								connectToPeer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
								sprintf(buffer, "FLOOD_FOR_ENTRIES %i R %s %s %s %s", elab.req_id,
										elab.aggr, elab.type, elab.p_date, elab.r_date);
								send_(addr);	
							}
							close_neig_response = 0;
						}
						//se ha entrambi i vicini
						if((strcmp(my_neighbors.left_neighbor_ip, "-") != 0) && (strcmp(my_neighbors.right_neighbor_ip, "-") != 0) && (close_neig_response == 2))
						{
							connectToPeer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
							sprintf(buffer, "FLOOD_FOR_ENTRIES %i L %s %s %s %s", elab.req_id,
									elab.aggr, elab.type, elab.p_date, elab.r_date);
							send_(addr);
						
							connectToPeer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
							sprintf(buffer, "FLOOD_FOR_ENTRIES %i R %s %s %s %s", elab.req_id,
									elab.aggr, elab.type, elab.p_date, elab.r_date);
							send_(addr);	
		
							close_neig_response = 0;
//printf("REPLY DATA CLOSE_NEIGH: %i\n", close_neig_response);
						}
					}
				}
			}

			if(strcmp(command, "FLOOD_FOR_ENTRIES") == 0) {
				//copia le variabili nella struct peer_req
				peer_req.req_id = atoi(first_arg);
				strcpy(side, second_arg);
				strcpy(peer_req.aggr, third_arg);
				strcpy(peer_req.type, fourth_arg);
				strcpy(peer_req.p_date, fifth_arg);
				strcpy(peer_req.r_date, sixth_arg);
				initCaseVariables(peer_req.p_date, peer_req.r_date);

				if((flooded == 1) || ((strcmp(my_neighbors.left_neighbor_ip, "-") == 0) || (strcmp(my_neighbors.right_neighbor_ip, "-") == 0))) {
					if((strcmp(my_neighbors.left_neighbor_ip, "-") == 0) || (strcmp(my_neighbors.right_neighbor_ip, "-") == 0)) {
//printf("SONO SOLO DUE\n");
						//calcola e salva il risultato per dopo
						if(strcmp(peer_req.aggr,"totale") == 0) {
printf("Calcolo totale\n");							
							getLocalTotal(1, peer_req.type);
						}
						if(strcmp(peer_req.aggr,"variazione") == 0) {
printf("Calcolo variazione\n");
							getLocalVariation(1, peer_req.type);
						}	

						//cerca risultato
						p = store.list;
						if(p != NULL) {
							if(p->id != peer_req.req_id) {					
								while(p->next) {
									if(p->id == peer_req.req_id)
										break;
									p = p->next;
								}
							}
						}
						if (p != NULL)	{	//ho le entry
//printf("P != NULL\n");
							if(strcmp(side, "L") == 0) 
								sprintf(buffer, "REPLY_FLOOD %i L %s", peer_req.req_id, my_port);
							if(strcmp(side, "R") == 0)
								sprintf(buffer, "REPLY_FLOOD %i R %s", peer_req.req_id, my_port);
						} else {
//printf("P == NULL\n");
							if(strcmp(side, "L") == 0) 
								sprintf(buffer, "REPLY_FLOOD %i L", peer_req.req_id);
							if(strcmp(side, "R") == 0)
								sprintf(buffer, "REPLY_FLOOD %i R", peer_req.req_id);					
						}
						//scelta del destinatario
						//se sono solo due
						if(strcmp(side, "R") == 0)	
							connectToPeer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
						if(strcmp(side, "L") == 0)
							connectToPeer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
					}
					if(flooded == 1) {
//printf("FLOODED == 1\n");
						//cerca risultato
						p = store.list;
						if(p != NULL) {
							if(p->id != peer_req.req_id) {					
								while(p->next) {
									if(p->id == peer_req.req_id)
										break;
									p = p->next;
								}
							}
						}
						if (p != NULL)	{	//ho le entry
//printf("P != NULL\n");
							if(strcmp(side, "L") == 0) 
								sprintf(buffer, "REPLY_FLOOD %i R 1 %s", peer_req.req_id, my_port);
							if(strcmp(side, "R") == 0)
								sprintf(buffer, "REPLY_FLOOD %i L 1 %s", peer_req.req_id, my_port);
						} else {
//printf("P == NULL\n");
							if(strcmp(side, "L") == 0) 
								sprintf(buffer, "REPLY_FLOOD %i R 0", peer_req.req_id);
							if(strcmp(side, "R") == 0)
								sprintf(buffer, "REPLY_FLOOD %i L 0", peer_req.req_id);					
						}
						//scelta del destinatario
						if(strcmp(side, "L") == 0) 	
							connectToPeer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
						if(strcmp(side, "R") == 0)
							connectToPeer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);

						flooded = 0;
					}
				} else {	//altrimenti inoltra FLOOD
printf("Inoltro flooding\n");
					//se ha entry aggiunge la propria porta e salva il 
					//risultato per dopo
					if(strcmp(peer_req.aggr,"totale") == 0) {
printf("Calcolo totale\n");
						getLocalTotal(1, peer_req.type);
					}
					if(strcmp(peer_req.aggr,"variazione") == 0) {
printf("Calcolo variazione\n");
						getLocalVariation(1, peer_req.type);
					}
					flooded = 1;
					strcpy(buffer, input);
					if(strcmp(side, "L") == 0) 	//inoltro
						connectToPeer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
					if(strcmp(side, "R") == 0) 	//inoltro
						connectToPeer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
				}
				send_(addr);
			}

			if(strcmp(command, "REPLY_FLOOD") == 0) {
				if(elab.req_id == atoi(first_arg)) {	//se è il peer richiedente
printf("Richiesta tornata al mittente\n");
//printf("REQ_ID: %i\nFIRST_ARG: %s\n", elab.req_id, first_arg);
					//caso due peer
					if((strcmp(my_neighbors.left_neighbor_ip, "-") == 0) || (strcmp(my_neighbors.right_neighbor_ip, "-") == 0)) {
						entry_gained = 0;
						connectToPeer(localhost, third_arg);
						sprintf(buffer, "REQ_ENTRIES %i %s", elab.req_id, my_port);
						send_(addr);
					} else {	//caso più di due peer
						close_neig_response++;
//printf("CLOSE_NEIGH: %i\n", close_neig_response);
						if(atoi(third_arg) > 0)
							sprintf(peer_to_contact+strlen(peer_to_contact), "%s-", fourth_arg);
printf("Peer to contact: %s\n", peer_to_contact);
						if(strcmp(second_arg, "L") == 0){
							counter_left = atoi(third_arg);
							counter += counter_left;
						}
						if(strcmp(second_arg, "R") == 0){
							counter_right = atoi(third_arg);
							counter += counter_right;
						}
						if(close_neig_response == 2)
						{
							if(strlen(peer_to_contact) == 0) {
								printf("No peer to contact\n");
							} else {
								entry_gained = 0;
								token = strtok(peer_to_contact, "-");
								sprintf(buffer, "REQ_ENTRIES %i %s", elab.req_id, my_port);
								while(token != NULL) {
									connectToPeer(localhost, token);
									sprintf(buffer, "REQ_ENTRIES %i %s", elab.req_id, my_port);
									send_(addr);
									token = strtok(NULL, "-");
								}
								strcpy(peer_to_contact, "");
							}
							close_neig_response = 0;
//printf("CLOSE_NEIGH: %i\n", close_neig_response);
						}
					}
				} else {		//inoltra REPLY_FLOOD
printf("Inoltro reply flood\n");
					strcpy(buffer, input);
					//cerco risultato
					p = store.list;
					if(p != NULL) {
						if(p->id != peer_req.req_id) {					
							while(p->next) {
								if(p->id == peer_req.req_id)
									break;
								p = p->next;
							}
						}
					}
					if (p != NULL)	{	//ha le entries
//printf("HO LE ENTRY\n");
						if(atoi(third_arg) == 0) {
							sprintf(fourth_arg, "%s", my_port);
						}
						else {
							strcat(fourth_arg, "-");
							strcat(fourth_arg, my_port);
						}
						i = atoi(third_arg)+1;
						sprintf(third_arg, "%i", i);
						sprintf(buffer, "REPLY_FLOOD %s %s %s %s", first_arg, second_arg, third_arg, fourth_arg);
					} else {	//non ha le entries
//printf("NON HO LE ENTRY\n");
						strcpy(buffer, input);
					}
					if(strcmp(side, "L") == 0) 
						connectToPeer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
					if(strcmp(side, "R") == 0)	
						connectToPeer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
					send_(addr);
					flooded = 0;
				}
			}

			if(strcmp(command, "REQ_ENTRIES") == 0) {
				//cerca il dato memorizzato 
				p = store.list;
				if(p->id == atoi(first_arg)) {			//risultato in testa
//printf("RISULTATO IN TESTA\n");
					sprintf(buffer, "REPLY_ENTRIES %i", p->id);
					if(strcmp(p->aggr, "totale") == 0) {
						sprintf(buffer+strlen(buffer), " %i %i", p->total, p->num_entries);
					}
					if(strcmp(p->aggr, "variazione") == 0) {
						sprintf(buffer+strlen(buffer), " %i %s", p->num_entries, p->variation);
					}
				} else {
//printf("ALTRIMENTI\n");
					while(p->next)  {
						if(p->next->id == atoi(first_arg))
							break;
						p = p->next;
					}
					sprintf(buffer, "REPLY_ENTRIES %i ", p->id);
					if(strcmp(p->aggr, "totale") == 0) {
						sprintf(buffer+strlen(buffer), "%i %i", p->next->total, p->next->num_entries);
					}
					if(strcmp(p->aggr, "variazione") == 0) {
						sprintf(buffer+strlen(buffer), "%i %s", p->next->num_entries, p->next->variation);
					}
				}
				connectToPeer(localhost, second_arg);
				send_(addr);
				printf("Store num: %i\n", store.num);
				printf("Stored: \n\t%i\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%i\n\t%i\n", store.list->id, store.list->aggr, store.list->type, store.list->p_date, store.list->r_date, store.list->variation, store.list->total, store.list->num_entries);

				if(store.num == 1){
					free(store.list->next);
					free(store.list);
					store.list = NULL;
				}
				else {printf("ELSE FREE\n");
					p->next = p->next->next;
					free(p->next);
					p->next = NULL;
				}
				store.num--;
				printf("Sore num: %i\n", store.num);
				flooded = 0;
			}

			if(strcmp(command, "REPLY_ENTRIES") == 0) {
				if(counter > 0){	//solo per più di due peer
					counter--;
//printf("COUNTER--   ->  %i\n", counter);
				}
				if(strcmp(elab.aggr, "totale") == 0) {
					tot_parziale += atoi(second_arg);
					entry_gained += atoi(third_arg);
//printf("ENTRY GAINED: %i\n", entry_gained);
				}
//printf("VAR_PARZIALE: %s\n", var_parziale);
				if(strcmp(elab.aggr, "variazione") == 0) {
					strcpy(buffer, input);
					token = strtok(buffer, " ");  //elimina REPLY_ENTRIES	
					token = strtok(NULL, " ");	//elimina id
					token = strtok(NULL, " ");	//elimina num_entries
					token = strtok(NULL, "\0");
					strcat(var_parziale, token);
					entry_gained += atoi(second_arg);
//printf("VAR_PARZIALE: %s\n", var_parziale);
				}
				
				if((counter == 0) || (strcmp(my_neighbors.left_neighbor_ip, "-") == 0) || 
					(strcmp(my_neighbors.right_neighbor_ip, "-") == 0)) 
				{
					if(strcmp(elab.aggr, "totale") == 0) {
						initCaseVariables(elab.p_date, elab.r_date);
						getLocalTotal(2, elab.type);
						tot_parziale = 0;
					}
					if(strcmp(elab.aggr, "variazione") == 0) {
						initCaseVariables(elab.p_date, elab.r_date);
						getLocalVariation(2, elab.type);
						strcpy(var_parziale, "");
					}
					if(entry_gained < entries_to_gain)
						printf("(Risultato parziale: alcuni peer non attivi)\n");
					counter_left = counter_right = 0;
				}
			}
		}

		if (FD_ISSET(0, &read_fds)) {  	//stdin pronto in lettura
			
			scanf("%[^\n]", buffer);
			scanf("%*c");
			parseString(buffer);

			if((strcmp(command, "start") == 0) && (valid_input == 1) && (peer_registered == 1)){
				printf("Errore: peer già registato presso il DS\n");
			}

			if((strcmp(command, "start") == 0) && (valid_input == 1) && (peer_registered == 0)){
				
				printf("Richiesta connessione al DS...\n");
				connectToDS(first_arg, second_arg);

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
							parseString(buffer);

							if(strcmp(command, "ACK") == 0) {
								peer_registered = 1;
								break;
							}
							if(strcmp(command, "MAX_EXC") == 0) {
								printf("Non è possibile registrarsi\n");
								closingActions();
								exit(0);
							}
						}

						if(pfds[0].revents) {	// può ricevere stop da terminale
							
							scanf("%[^\n]", buffer);
							scanf("%*c");
							parseString(buffer);
							
							if((strcmp(command, "stop") == 0) && (valid_input == 1)) {
								printf("Terminazione forzata\n");
								closingActions();
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
			}

			if((strcmp(command, "add") == 0) && (valid_input == 1)) {
				if(peer_connected == 0)
					printf("Errore: peer non connesso al DS\n");
				else if((strcmp(first_arg, "N") != 0) && (strcmp(first_arg, "T") != 0))
					printf("Formato invalido, digitare: add [N|T] quantità\n");
				else {				
					if(strcmp(first_arg, "N") == 0) 
						tot.nuoviCasi++;
					if (strcmp(first_arg, "T") == 0) 
						tot.tamponi++;
					printf("Num entry odierne: tamponi: %i nuovi casi: %i\n",
						tot.tamponi, tot.nuoviCasi);
					//salvataggio su file
					updateRegister();
				}
			}	

			if((strcmp(command, "get") == 0) && (valid_input == 1)) {
printf("-------------------------- get request --------------------------\n");
				valid_period = parsePeriod(third_arg);
				if((strcmp(first_arg, "totale") != 0 && strcmp(first_arg, "variazione") != 0) ||
				    (strcmp(second_arg, "N") != 0) && (strcmp(second_arg, "T") != 0) ||
					(valid_period == 0)) 
				{					
					printf("Formato invalido, digitare: ");
					printf("get totale|variazione N|T dd1:mm1:yyyy1-dd2:mm2:yyyy2\n");

				} else {
					elab.req_id = rand();
					//compilo struct Request
					strcpy(elab.aggr, first_arg);
					strcpy(elab.type, second_arg);
				
					sprintf(buffer, "GET %s %s %s", elab.type, elab.p_date, elab.r_date);
					send_(srv_addr);

					receive_(srv_addr);
					strcpy(buffer_tmp, buffer);

					if((strcmp(elab.aggr, "totale") == 0) && (strcmp(elab.r_date, "*") != 0))
						i = searchInCache(cacheTotale, 0);
					if((strcmp(elab.aggr, "variazione") == 0) && (strcmp(elab.r_date, "*") != 0))
						i = searchInCache(cacheVariazione, 0);
					if((i == 0) || (strcmp(elab.r_date, "*") == 0)){
						initCaseVariables(elab.p_date, elab.r_date);
						floodingToDo();
printf("Flooding = %i\n", flooding);
						if(flooding == 0) {
							if(strcmp(elab.aggr, "totale") == 0) {
								initCaseVariables(elab.p_date, elab.r_date);
								//calcolo dato e memorizzo il dato
								getLocalTotal(0, elab.type);
							}
							if(strcmp(elab.aggr, "variazione") == 0) {
								initCaseVariables(elab.p_date, elab.r_date);
								getLocalVariation(0, elab.type);
							}
						}
						if(flooding == 1) {
							if((strcmp(my_neighbors.left_neighbor_ip, "-") == 0) && (strcmp(my_neighbors.right_neighbor_ip, "-") == 0))
								printf("No neighbors\n");
							if(strcmp(my_neighbors.left_neighbor_ip, "-") != 0) {
								connectToPeer(my_neighbors.left_neighbor_ip, my_neighbors.left_neighbor_port);
								sprintf(buffer, "REQ_DATA %s %s %s %s %s", my_port, elab.aggr, elab.type, elab.p_date, elab.r_date);
								send_(addr);
//printf("INVIATO A LEFT: %s\n", my_neighbors.left_neighbor_port);
							}
							if(strcmp(my_neighbors.right_neighbor_ip, "-") != 0) {
								connectToPeer(my_neighbors.right_neighbor_ip, my_neighbors.right_neighbor_port);
								sprintf(buffer, "REQ_DATA %s %s %s %s %s", my_port, elab.aggr, elab.type, elab.p_date, elab.r_date);
								send_(addr);
//printf("INVIATO A RIGHT %s\n", my_neighbors.right_neighbor_port);
							}
						}
					}
				}
			}	
			if((strcmp(command, "stop") == 0) && (valid_input == 1)) {	
			
				if(peer_connected) {
					sprintf(buffer, "QUIT %s %s", localhost, my_port);
					send_(srv_addr);
				}
				printf("Terminazione forzata\n");
				closingActions();
				exit(0);
			}
		}	
	} //while
	closingActions();
} //main