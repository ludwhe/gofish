/*
 * goproxy.c - a gopher proxy server for Opera
 * Copyright (C) 2002 Sean MacLennan <seanm@seanm.ca>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * We rely heavily on the fact that Opera tries to handle bad html!
 *
 * Nice to do: Opera support presistenct connects. Keep the opera
 * connection open and only close the remote connection. Would require
 * a poll and/or select.
 */

/* We do not handle 7 type urls */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "gopherd.h"

int verbose = 0;

// From mm-utils.c
int listen_socket(int port);
int accept_socket(int sock, unsigned *addr);

// forward references
static void catfish(void);
static int connect_socket(int port, unsigned addr);
static unsigned gethostaddr(char *hostname);


#define SERVER_STRING	"Server: catfish/0.1\r\n"


void sighandler(int signum)
{
	switch(signum) {
	case SIGHUP:
	case SIGTERM:
	case SIGINT:
		// Somebody wants us to quit
		printf("KILLED\n");
		exit(0);
	case SIGPIPE:
		printf("Somebody closed on us\n");
		break;
	default:
		printf("Got an unexpected %d signal\n", signum);
		break;
	}
}


int main(int argc, char *argv[])
{
	pid_t pid;
	int c, daemon = 0;

	while((c = getopt(argc, argv, "dv")) != -1)
		switch(c) {
		case 'd': daemon = 1; break;
		case 'v': ++verbose; break;
		default:
			printf("usage: %s [-dv]\n", *argv);
			exit(1);
		}

	if(daemon) {
		if((pid = fork()) == 0) {
			catfish(); // never returns
			exit(1);	// paranoia
		}
		return pid ==-1 ? 1 : 0; // parent exits
	}
	else
		catfish(); // never returns
	return 1; // compiler shutup
}


int write_out(int sock, char *buf, int len)
{
	static char cache[4096];
	static int clen = 0;
	int n;

	if(len == 0) {
		// flush
		if(clen) {
			write(sock, cache, clen);
			clen = 0;
		}
		return 0;
	}

	while(len > 0) {
		n = sizeof(cache) - clen;
		n = len > n ? n : len;
		memcpy(&cache[clen], buf, n);
		len -= n;
		clen += n;
		if(clen == sizeof(cache)) {
			write(sock, cache, clen);
			clen = 0;
		}
	}

	return 0;
}


int write_str(int sock, char *str)
{
	return write_out(sock, str, strlen(str));
}


void send_response(int sock, int code, char type)
{
	char response[100];

	sprintf(response, "HTTP/1.1 %3d ", code);

	switch(code) {
	case 200: strcat(response, "OK"); break;
	case 400: strcat(response, "Bad Request"); break;
	case 404: strcat(response, "Not Found"); break;
	case 500: strcat(response, "Internal Server Error"); break;
	default:  strcat(response, "Unknown Error"); break;
	}

	strcat(response, "\r\n");

	write(sock, response, strlen(response));

	write(sock, SERVER_STRING, strlen(SERVER_STRING)); // optional

	write(sock, "\r\n", 2);
}


void get_directory(int osock, int sock)
{
	FILE *fp;
	char buf[1024], *p, *url, *host;
	int port;
	char str[1024];

	if(!(fp = fdopen(osock, "r"))) {
		perror("fdopen");
		return;
	}

	write_str(sock, "<html><head><title>CatFish</title></head>\n");
	write_str(sock, "<body><center>\n<table>\n");

	while(fgets(buf, sizeof(buf), fp)) {
		if(strcmp(buf, ".\r\n") == 0) break;
		if(!(p = strchr(buf, '\t'))) {
			printf("Bad line %s", buf);
			continue;
		}
		*p++ = '\0';
		url = p;
		if(!(p = strchr(url, '\t'))) {
			printf("Bad line 2 %s", buf);
			continue;
		}
		*p++ = '\0';
		host = p;
		if(!(p = strchr(host, '\t'))) {
			printf("Bad line 2 %s", buf);
			continue;
		}
		*p++ = '\0';
		port = strtol(p, NULL, 10);

		if(*buf == 'i') { // informational
			write_out(sock, "<tr><td>", 8);
			write_out(sock, buf + 1, strlen(buf + 1));
			write_out(sock, "\n", 1);
			continue;
		}

		write_out(sock, "<tr><td><a href=\"", 17);
		if(port != 70)
			sprintf(str, "gopher://%s:%d/%c%s", host, port, *buf, url);
		else
			sprintf(str, "gopher://%s/%c%s", host, *buf, url);
		write_out(sock, str, strlen(str));

		write_out(sock, "\">", 2);
		write_out(sock, buf + 1, strlen(buf) - 1);
		write_out(sock, "</a>\n", 5);
	}

	write_str(sock, "</table>\n</center></body></html>\n");

	write_out(sock, NULL, 0);

	fclose(fp);
}


// Note: We never expand an url, only extract or copy as is
// Not done yet...
void unquote_url(char *url)
{
	char *str, *p;
	int did_something = 0;

	if((str = strdup(url)) == NULL) return;

	while((p = strchr(str, '%')))
		if(strncmp(p, "%20", 3) == 0) {
			*p++ = ' ';
			// memmove allows overlapping areas
			// remember to copy the null!
			memmove(p, p + 2, strlen(p + 2) + 1);
			++did_something;
		}

	if(did_something) strcpy(url, str);

	free(str);
}


void docmd(int sock, char *cmd)
{
	char *p, *e;
	int port = 70;
	unsigned ip;
	int osock;
	char buf[4096];
	int n;

	if(strncmp(cmd, "GET gopher://", 13)) {
		send_response(sock, 400, 'h');
		printf("Bad command\n");
		return;
	}
	cmd += 13;

	// isolate the host
	for(p = cmd; *p && *p != '/' && *p != ':'; ++p) ;

	if(*p == ':') {
		*p++ = '\0';
		port = strtol(p, &p, 10);
		if(*p == '/') ++p;
	}
	else if(*p == '/')
		*p++ = '\0';

	// SAM this assumes no spaces in URLS
	// SAM also assumes no quoting
	for(e = p; *e && !isspace((int)*e); ++e) ;
	*e = '\0';
	unquote_url(p);

	if(!(ip = gethostaddr(cmd))) {
		send_response(sock, 404, 'h');
		printf("Bad address\n");
		return;
	}

	printf("host %s (%s) url %s port %d\n", cmd, ntoa(ip), p, port);

	if((osock = connect_socket(port, ip)) < 0) {
		send_response(sock, 404, 'h');
		perror("connect\n");
		return;
	}

	send_response(sock, 200, *p);

	// SAM this should be one write
	write(osock, p + 1, strlen(p) - 1);  // SAM HACK skip first char...
	write(osock, "\r\n", 2);

	if(*p == '1' || *p == '\0')
		get_directory(osock, sock);
	else {
		while((n = read(osock, buf, sizeof(buf))) > 0)
			write(sock, buf, n);
		close(osock);
	}

}

void catfish(void)
{
	int csock, sock;
	unsigned addr;

	openlog("catfish", LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Catfish started.");



	signal(SIGHUP,  sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGINT,  sighandler);
	signal(SIGPIPE, sighandler);


	// connection socket
	if((csock = listen_socket(7007)) < 0) {
		printf("Unable to create listen socket\n");
		syslog(LOG_ERR, "Unable to create socket\n");
		exit(1);
	}

	while(1) {
		char buf[2048], *p;
		int n, len, bytes;

		if((sock = accept_socket(csock, &addr)) < 0) {
			syslog(LOG_WARNING, "unable to accept new connection: %m");
			continue;
		}

		printf("Got a connection!!!\n"); // SAM DBG

		p = buf;
		len = sizeof(buf) - 1;
		bytes = 0;
		while((n = read(sock, p, len)) > 0) {
			p[n] = '\0';
			printf("read %d\n", n);
			bytes += n;
			if(bytes > 4) {
				if(strcmp(buf + bytes - 4, "\r\n\r\n") == 0) {
					docmd(sock, buf);
					break;
				}
			}
			p += n;
			len -= n;
		}

		close(sock);
	}
}


int connect_socket(int port, unsigned addr)
{
	struct sockaddr_in sock_name;
	int sock;

	if((sock = go_socket (AF_INET, SOCK_STREAM, 0)) == -1)
		return -1;

	memset(&sock_name, 0, sizeof(sock_name));
	sock_name.sin_family = AF_INET;
	sock_name.sin_addr.s_addr = addr;
	sock_name.sin_port = htons(port);

	if(connect(sock, (struct sockaddr *)&sock_name, sizeof(sock_name))) {
		close(sock);
		return -1;
	}

	return sock;
}


unsigned gethostaddr(char *hostname)
{
	struct hostent *host;

	if((host = gethostbyname(hostname)) == NULL)
		return 0;

	return *(unsigned *)host->h_addr_list[0];
}

/*
 * Local Variables:
 * compile-command: "gcc -O3 -Wall goproxy.c socket.o -o goproxy"
 * End:
 */
