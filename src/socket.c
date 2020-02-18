/*
 * socket.c - socket utilities for the gofish gopher daemon
 * Copyright (C) 2000,2002  Sean MacLennan <seanm@seanm.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GoFish; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
 * All knowledge of sockets should be isolated to this file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "gofish.h"

/* We cannot define this anywhere else */
static in_addr_t listen_addr = INADDR_ANY;

void set_listen_address(char *addr)
{
	listen_addr = inet_addr(addr);
}

int listen_socket(int port)
{
	struct sockaddr_in sock_name;
	int s, optval;

	sock_name.sin_family = AF_INET;
	sock_name.sin_addr.s_addr = listen_addr; /* already network addr */
	sock_name.sin_port = htons(port);
	optval = 1;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -1;

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
				   (char *)&optval, sizeof(optval)) == -1 ||
		bind(s, (struct sockaddr *)&sock_name, sizeof(sock_name)) == -1 ||
		listen(s, GOPHER_BACKLOG) == -1)
	{
		close(s);
		return -1;
	}

	if ((optval = fcntl(s, F_GETFL, 0)) == -1 ||
		fcntl(s, F_SETFL, optval | O_NONBLOCK))
	{
		close(s);
		return -1;
	}

	return s;
}

int accept_socket(int sock, unsigned *addr)
{
	struct sockaddr_in sock_name;
	int addrlen = sizeof(sock_name);
	int new, flags;

	if ((new = accept(sock, (struct sockaddr *)&sock_name, &addrlen)) < 0)
		return -1;

	if (addr)
		*addr = htonl(sock_name.sin_addr.s_addr);

	flags = fcntl(new, F_GETFL, 0);
	if (flags == -1 || fcntl(new, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		printf("fcntl failed\n");
		close(new);
		return -1;
	}

	flags = 1;
	if (setsockopt(new, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags)))
		perror("setsockopt(TCP_NODELAY)"); // not fatal

	return new;
}

// network byte order
char *ntoa(unsigned n)
{
	static char a[16];

	sprintf(a, "%d.%d.%d.%d",
			(n >> 24) & 0xff,
			(n >> 16) & 0xff,
			(n >> 8) & 0xff,
			n & 0xff);

	return a;
}
