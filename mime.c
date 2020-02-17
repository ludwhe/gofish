/*
 * mime.c - mime file support for GoFish
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gofish.h"


struct mime {
	char *ext;
	char *mime;
};


static char *mime_file = "/etc/mime.types";
static int mime_set = 0;

static struct mime *mimes = NULL;
static int n_mimes = 0;


void set_mime_file(char *fname)
{
	mime_file = strdup(fname);
	mime_set = 1;
}


char *mime_find(char *fname)
{
	char *ext;
	struct mime *m;
	int i;

	if((ext = strrchr(fname, '.'))) {
		++ext;

		for(m = mimes, i = 0; i < n_mimes; ++i, ++m)
			if(strcmp(m->ext, ext) == 0)
				return m->mime;
	}

	return NULL;
}


static int add_mime(char *mime, char *ext)
{
	struct mime *m;
	int i;

	// See if extension already exists
	for(m = mimes, i = 0; i < n_mimes; ++i, ++m)
		if(strcmp(m->ext, ext) == 0)
			return 1;

	if(!(mimes = realloc(mimes, (n_mimes + 1) * sizeof(struct mime)))) {
		printf("Out of memory\n");
		exit(1);
	}

	m = mimes + n_mimes;
	++n_mimes;

	m->mime = must_strdup(mime);
	m->ext  = must_strdup(ext);

	return 0;
}


static int read_mime_file(char *fname)
{
	FILE *fp;
	char line[160], *p, *e;

	if((fp = fopen(fname, "r")) == NULL) {
		if(mime_set) perror(fname);
		return 1;
	}

	while(fgets(line, sizeof(line), fp)) {
		if(*line == '#' || isspace((int)*line)) continue; // comment
		if((p = strchr(line, '\n'))) *p = '\0';

		for(p = line; *p && !isspace((int)*p); ++p) ;
		if(*p == '\0') continue;
		*p++ = '\0';
		while(isspace((int)*p)) ++p;

		for(e = p; *e; p = e) {
			while(*e && !isspace((int)*e)) ++e;
			if(*e) {
				*e++ = '\0';
				while(isspace((int)*e)) ++e;
			}

			add_mime(line, p);
		}
	}

	fclose(fp);

	return 0;
}


static struct mime default_mimes[] = {
	{ "gif",  "image/gif"  },
	{ "jpg",  "image/jpeg" },
	{ "png",  "image/png"  },
	{ "jpeg", "image/jpeg" },
	{ "pdf",  "application/pdf" },
	{ "html", "text/html" },
	{ "htm",  "text/html" },
	{ "txt",  "text/plain" },
	{ "ico",  "text/plain" },
	{ 0, 0 },
};


void mime_init()
{
	struct mime *m;

	if(mime_file)
		read_mime_file(mime_file);

	if(mime_set) free(mime_file);

	// Add the defaults if not already set
	for(m = default_mimes; m->ext; ++m)
		add_mime(m->mime, m->ext);
}


void mime_cleanup()
{
	struct mime *m;
	int i;

	for(m = mimes, i = 0; i < n_mimes; ++i, ++m) {
		free(m->mime);
		free(m->ext);
	}

	free(mimes);
}
