#include <stdio.h>
#include <stdlib.h>

#define BUFFER_LEN    1024
#define ADDR_LEN	  10
#define PORT_LEN      16
#define CMD_LEN		  25

struct Boot {
	char ip[ADDR_LEN];
	char port[PORT_LEN];
};

struct Request {		//dal peer al DS
	char cmd[CMD_LEN];
	int howmany;
};


