#include <stdio.h>
#include <stdint.h>

#define ALLOWED_DIR "./website"

#include "html.h"


coroutine void server(){
	const char* host = "0.0.0.0";
	int port = 5000;

	ipaddr address = iplocal(host, port, 0);
	tcpsock ls = tcplisten(address, 255);
	while(1){
		tcpsock s = tcpaccept(ls, -1);
		go(html_client(s));
	}
}

int main() {
	printf("Starting server coroutine\n");
    go(server());
    msleep(-1);
    return 0;
}
