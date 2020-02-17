/*
 * webtest - Tests the gofish gopher/http daemon
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
 * along with XEmacs; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

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

#include "gofish.h"

/*
 * This is a currently very incomplete set of tests for GoFish. They
 * should work with other web/gopher servers. However, the status
 * returns may not be exactly the same.
 *
 * Please add more tests and send my a patch, or the complete source!
 */

#define HTTP_PORT	80
#define GOPHER_PORT	70

int connect_socket(int port, unsigned addr);
unsigned gethostaddr(char *hostname);
int http_reply(int sock, char *buf, int bufsize);
int gopher_reply(int sock, char *buf, int size);


int main(int argc, char *argv[])
{
	char *hostname = "localhost";
	unsigned hostaddr;
	int sock, rc;
	char buf[MAX_LINE * 2], *p;
	int tests = 0, ok = 0, dubious = 0;
	unsigned mask = 7;

	if(argc > 1) hostname = argv[1];
	if(argc > 2) mask = strtol(argv[2], 0, 0);

 redirect:
	if((hostaddr = gethostaddr(hostname)) == 0) {
		printf("%s unknown host\n", hostname);
		exit(1);
	}

	// ======================================================================
	// HTTP
	// ======================================================================

	if((mask & 1) == 0) goto gopher;

	printf("HTTP tests:\n");

	// ----------------------------------------------------------------------
	// Try a simple request - handles redirects
	++tests;
	if((sock = connect_socket(HTTP_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("Is http server running?\n");
		goto gopher;
	}

	sprintf(buf, "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", hostname);

	write(sock, buf, strlen(buf));

	switch((rc = http_reply(sock, buf, sizeof(buf)))) {
	case 200:
		++ok;
		printf("GoFish expected reply %d\n", 200);
		break; // ok
	case 300:
	case 301:
	case 302:
	case 303:
	case 304:
	case 305:
		if((p = strstr(buf, "Location: "))) {
			char *e;

			p += 10;
			while(isspace((int)*p)) ++p;
			if(strncmp(p, "http:/", 6) == 0) p += 6;
			if(*p == '/') ++p;
			if((e = strchr(p, '\n')) == NULL) {
				printf("Bad redirect\n");
				exit(1);
			}
			while(isspace((int)*(e - 1))) --e;
			if(e <= p) {
				printf("Bad redirect\n");
				exit(1);
			}
			if(*(e - 1) == '/') --e;
			*e = '\0';
			printf("Redirect to '%s'\n", p);
			hostname = strdup(p);
			goto redirect;
		}
		else {
			printf("Redirect with no location\n");
			exit(1);
		}
		break;
	case -1:
		// we already printed the error
		exit(1);
	default:
		printf("Error return %d\n", rc);
		exit(1);
	}

	// ----------------------------------------------------------------------
	// Try a large request
	++tests;
	if((sock = connect_socket(HTTP_PORT, hostaddr)) < 0) {
		perror("connect");
		exit(1);
	}

	strcpy(buf, "GET /");
	p = buf + strlen(buf);
	memset(p, 'a', 500);
	p += 500;
	sprintf(p, " HTTP/1.0\r\nHost: %s\r\n\r\n", hostname);

	write(sock, buf, strlen(buf));

	rc = http_reply(sock, buf, sizeof(buf));

	if(rc == 404) {
		++ok;
		printf("GoFish expected reply %d\n", rc);
	} else if(rc >= 400 && rc < 500) {
		++dubious;
		printf("Got an expected error reply %d\n", rc);
	} else if(rc == 200)
		printf("Huh? Got an OK\n");
	else if(rc != -1)
		printf("Unexpected return %d\n", rc);

	// ----------------------------------------------------------------------
	// Premature close - we cannot check the results...
	// For GoFish, you should see a 408 error in the log
	++tests;
	++ok;
	if((sock = connect_socket(HTTP_PORT, hostaddr)) < 0) {
		perror("connect");
		exit(1);
	}

	strcpy(buf, "GET /premature/close HTTP/1.0\r\nHost: ");

	write(sock, buf, strlen(buf));

	close(sock);

	// ----------------------------------------------------------------------
	// Try a huge request > MAX_LINE
	++tests;
	if((sock = connect_socket(HTTP_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("The premature close test probably killed the server!\n");
		exit(1);
	}

	strcpy(buf, "GET /");
	p = buf + strlen(buf);
	memset(p, 'a', MAX_LINE + 1);
	p += MAX_LINE + 1;
	sprintf(p, " HTTP/1.0\r\nHost: %s\r\n\r\n", hostname);

	write(sock, buf, strlen(buf));

	rc = http_reply(sock, buf, sizeof(buf));

	if(rc == 414) {
		++ok;
		printf("GoFish expected reply %d\n", rc);
	} else if(rc >= 400 && rc < 500) {
		++dubious;
		printf("Got an expected error reply %d\n", rc);
	} else if(rc == 200)
		printf("Huh? Got an OK\n");
	else if(rc != -1)
		printf("Unexpected return %d\n", rc);


	// ----------------------------------------------------------------------
	// Die during a server write - need the log to see if it really worked.
	// Should see a 504 error for k10files.tar
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("The premature close test probably killed the server!\n");
		exit(1);
	}

	sprintf(buf, "GET k10files.tar HTTP/1.0\r\nHost: %s\r\n\r\n", hostname);

	write(sock, buf, strlen(buf));

	switch((rc = http_reply(sock, buf, 20))) {
	case 200: ++ok; printf("GoFish expected reply %d\n", rc); break;
	case 404:
		++dubious;
		printf("Request failed, k10files.tar probably does not exist.\n");
		break;
	case -1: break; // already printed error
	default: printf("Unexpected error result %d\n", rc); break;
	}

	// ======================================================================
	// GOPHER
	// ======================================================================
 gopher:

	if((mask & 2) == 0) goto gateway;

	printf("\nGopher tests:\n");

	// For cut and paste errors
#undef HTTP_PORT

	// ----------------------------------------------------------------------
	// Try the simplest request
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("Is gopherd running?\n");
		goto done;
	}

	write(sock, "\r\n", 2);

	switch((rc = gopher_reply(sock, buf, sizeof(buf)))) {
	case 0: ++ok; printf("GoFish expected reply\n"); break;
	case 1: printf("Request failed %s", buf); break;
	case -1: break; // already printed error
	default: printf("HUH? %d\n", rc);
	}

	// ----------------------------------------------------------------------
	// Try a large request
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		exit(1);
	}

	strcpy(buf, "0/");
	p = buf + strlen(buf);
	memset(p, 'a', 500);
	p += 500;
	strcpy(p, "\r\n");

	write(sock, buf, strlen(buf));

	switch((rc = gopher_reply(sock, buf, sizeof(buf)))) {
	case 3: ++ok; printf("GoFish expected reply\n"); break;
	case 0: printf("Success not expected\n"); break;
	case -1: break; // already printed error
	default: printf("HUH? %d\n", rc);
	}

#if 1
	// ----------------------------------------------------------------------
	// Premature close - we cannot check the results...
	// For GoFish, you should see a 408 error in the log
	++tests;
	++ok;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		exit(1);
	}

	strcpy(buf, "0/premature/close");

	write(sock, buf, strlen(buf));

	close(sock);
#endif

#if 1
	// ----------------------------------------------------------------------
	// Try a huge request > MAX_LINE
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("The premature close test probably killed the server!\n");
		exit(1);
	}

	strcpy(buf, "1/");
	p = buf + strlen(buf);
	memset(p, 'a', MAX_LINE + 1);
	p += MAX_LINE + 1;
	strcpy(p, "\r\n");

	write(sock, buf, strlen(buf));

	switch((rc = gopher_reply(sock, buf, sizeof(buf)))) {
	case 414: ++ok; printf("GoFish expected reply\n"); break;
	case 0: printf("Success not expected\n"); break;
	case 3: ++dubious; printf("Request failed %s", buf); break;
	case -1: break; // already printed error
	default: ++dubious; printf("Expected error result %d\n", rc); break;
	}
#endif

#if 1
	// ----------------------------------------------------------------------
	// Die during a server write - need the log to see if it really worked.
	// Should see a 504 error for k10files.tar
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("The premature close test probably killed the server!\n");
		exit(1);
	}

	strcpy(buf, "9/k10files.tar\r\n");

	write(sock, buf, strlen(buf));

	switch((rc = gopher_reply(sock, buf, 10))) {
	case 0: ++ok; printf("GoFish expected reply\n"); break;
	case 3:
		++dubious;
		printf("Request failed, k10files.tar probably does not exist.\n");
		break;
	case -1: break; // already printed error
	default: printf("Expected error result %d\n", rc); break;
	}
#endif

	// ======================================================================
	// HTTP to GOPHER GATEWAY
	// ======================================================================

 gateway:

	if((mask & 4) == 0) goto done;

	printf("\nGateway tests:\n");

	// ----------------------------------------------------------------------
	// Try the simplest request
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("Is gopherd running?\n");
		goto done;
	}

	write(sock, "GET / HTTP/1.0\r\n\r\n", 18);

	rc = http_reply(sock, buf, sizeof(buf));

	switch(rc) {
	case 200: ++ok; printf("GoFish expected reply %d\n", rc); break;
	case -1: break; // already printed error
	default: printf("Unexpected error return %d\n", rc); break;
	}

#if 1
	// ----------------------------------------------------------------------
	// Try a large request
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		exit(1);
	}

	strcpy(buf, "GET 0/");
	p = buf + strlen(buf);
	memset(p, 'a', 500);
	p += 500;
	strcpy(p, " HTTP/1.0\r\n\r\n");

	write(sock, buf, strlen(buf));

	rc = http_reply(sock, buf, sizeof(buf));

	if(rc == 404) {
		++ok;
		printf("GoFish expected reply %d\n", rc);
	} else if(rc >= 400 && rc < 500) {
		++dubious;
		printf("Got an expected error reply %d\n", rc);
	} else if(rc == 200)
		printf("Huh? Got an OK\n");
	else if(rc != -1)
		printf("Unexpected return %d\n", rc);
#endif

#if 1
	// ----------------------------------------------------------------------
	// Premature close - we cannot check the results...
	// For GoFish, you should see a 408 error in the log
	++tests;
	++ok;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		exit(1);
	}

	strcpy(buf, "GET /premature/close HTTP/1.0\r\nHost: ");

	write(sock, buf, strlen(buf));

	close(sock);

	// ----------------------------------------------------------------------
	// Try a huge request > MAX_LINE
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("The premature close test probably killed the server!\n");
		exit(1);
	}

	strcpy(buf, "GET /");
	p = buf + strlen(buf);
	memset(p, 'a', MAX_LINE + 1);
	p += MAX_LINE + 1;
	strcpy(p, " HTTP/1.0\r\n\r\n");

	write(sock, buf, strlen(buf));

	rc = http_reply(sock, buf, sizeof(buf));

	if(rc == 414) {
		++ok;
		printf("GoFish expected reply %d\n", rc);
	} else if(rc >= 400 && rc < 500) {
		++dubious;
		printf("Got an expected error reply %d\n", rc);
	} else if(rc == 200)
		printf("Huh? Got an OK\n");
	else if(rc != -1)
		printf("Unexpected return %d\n", rc);

	// ----------------------------------------------------------------------
	// Die during a server write - need the log to see if it really worked.
	// Should see a 504 error for k10files.tar
	++tests;
	if((sock = connect_socket(GOPHER_PORT, hostaddr)) < 0) {
		perror("connect");
		printf("The premature close test probably killed the server!\n");
		exit(1);
	}

	strcpy(buf, "GET 9/k10files.tar HTTP/1.0\r\n\r\n");

	write(sock, buf, strlen(buf));

	switch((rc = http_reply(sock, buf, 20))) {
	case 200: ++ok; printf("GoFish expected reply %d\n", rc); break;
	case 404:
		++dubious;
		printf("Request failed, k10files.tar probably does not exist.\n");
		break;
	case -1: break; // already printed error
	default: printf("Unexpected error result %d\n", rc); break;
	}
#endif

	// ----------------------------------------------------------------------
 done:
	printf("\nRESULTS:\n  Tests   %3d\n  OK      %3d\n"
		   "  Dubious %3d\n  Failed    %d\n\n",
		   tests, ok, dubious, tests - ok - dubious);

	return 0;
}


int gopher_reply(int sock, char *buf, int size)
{
	int n;
	char *p, *e;

	n = read(sock, buf, size - 1);
	close(sock);

	if(n <= 0) {
		perror("read");
		return -1;
	}
	buf[n] = '\0';

	if(*buf == '3') {
		if((p = strchr(buf, '['))) {
			++p;
			n = strtol(p, &e, 10);
			if(*e == ']') return n;
		}
		// default
		return 3;
	}

	return 0;
}


int http_reply(int sock, char *buf, int size)
{
	int n, status;

	n = read(sock, buf, size - 1);
	close(sock);

	if(n <= 0) {
		perror("read");
		return -1;
	}
	buf[n] = '\0';

	if(strncmp(buf, "HTTP/1.1 ", 9) &&
	   strncmp(buf, "HTTP/1.0 ", 9)) {
		printf("Bad status\n");
		return -1;
	}

	status = strtol(buf + 9, 0, 10);

	if(status < 100 || status >= 600) {
		printf("Bad status %d\n", status);
		return -1;
	}

	return status;
}


int connect_socket(int port, unsigned addr)
{
	struct sockaddr_in sock_name;
	int sock;

#undef socket
	if((sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
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
