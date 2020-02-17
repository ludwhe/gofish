/*
 * gmap2cache - converts gophermap files to .cache files
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
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#include "gofish.h"


int verbose = 0;
int recurse = 0;

int mmap_cache_size; // needed by config

int process_dir(char *path, int level);
int process_file(char *dir, int level);
int read_dir(char *path, int level);


#define GOPHERMAP	"/gophermap"
#define CACHE		"/.cache"


char portstr[11];

int main(int argc, char *argv[])
{
	char *dir = NULL;
	char *config = GOPHER_CONFIG;
	char full[PATH_MAX];
	int c;
	int level;

	while((c = getopt(argc, argv, "c:rv")) != -1)
		switch(c) {
		case 'c': config = strdup(optarg); break;
		case 'r': recurse = 1; break;
		case 'v': ++verbose; break;
		default:
			printf("usage: %s [-rv] [dir]\n", *argv);
			exit(1);
		}

	read_config(config);

	sprintf(portstr, "%d", port);

	if(!realpath(root_dir, full)) {
		perror(root_dir);
		exit(1);
	}
	if(strcmp(root_dir, full)) {
		free(root_dir);
		if((root_dir = strdup(full)) == NULL) {
			printf("Out of memory\n");
			exit(1);
		}
	}

	if(chdir(root_dir)) {
		perror(root_dir);
		exit(1);
	}

	if(optind < argc) {
		dir = argv[optind];
		if(!realpath(dir, full)) {
			perror(dir);
			exit(1);
		}
		if(strncmp(root_dir, full, strlen(root_dir))) {
			printf("%s is not a subdir of %s\n", dir, root_dir);
			exit(1);
		}
		dir = full + strlen(root_dir);
		if(*dir == '/') ++dir;
	}

	if(dir == NULL || *dir == '\0')
		dir = ".";

	if(verbose > 1)
		printf("hostname '%s' port '%d'\nbase '%s' dir '%s'\n",
			   hostname, port, root_dir, dir);

	level = strcmp(dir, ".") ? 1 : 0;
	process_dir(dir, level);

	// This is for valgrind and will not be 100% correct if you
	// have anything other than a stock gofish.conf
	free(root_dir);
	free(hostname);

	return 0;
}


// Process one directory
int process_dir(char *path, int level)
{

	if(verbose) printf("Processing [%d] %s\n", level, path);

	if(process_file(path, level))
		printf("Process file failed for %s\n", path);

	if(recurse)
		read_dir(path, level);

	return 0;
}


// Convert a gophermap to a .cache
int process_file(char *dir, int level)
{
	FILE *in, *out;
	char buf[PATH_MAX];
	char tmp[PATH_MAX];
	char *field[4], *p;
	int i, len = strlen(dir);
	int err;

	if(len + 10 + 1 >= PATH_MAX) {
		printf("%s: too long\n", dir);
		return 1;
	}

	strcpy(buf, dir);
	strcat(buf, GOPHERMAP);
	if((in = fopen(buf, "r")) == NULL) {
		if(errno == ENOENT) {
			if(verbose) printf("%s does not exist\n", buf);
			return 0;
		}
		perror(buf);
		return 1;
	}

	strcpy(buf, dir);
	strcat(buf, CACHE);
	if((out = fopen(buf, "w")) == NULL) {
		fclose(in);
		perror(buf);
		return 1;
	}

	while(fgets(buf, sizeof(buf), in)) {
		if((p = strchr(buf, '\n'))) {
			if(p > buf && *(p - 1) == '\r') --p;
			*p = '\0';
		}
		p = buf;
		for(i = 0; i < 3; ++i) {
			field[i] = p;
			while(*p && *p != '\t') ++p;
			if(*p) *p++ = '\0';
		}
		field[i] = p;

		if(*buf == '\0') continue; // empty

		if(!*field[3]) field[3] = portstr;
		if(!*field[2]) field[2] = hostname;
		if(!*field[1]) {
			if(*field[0] == 'i')
				field[1] = "/fake";
			else
				field[1] = field[0];
		}

		if(*(field[1] + 1) != '/') {
			if(level == 0)
				sprintf(tmp, "%c/%s", *field[1], field[1] + 1);
			else
				sprintf(tmp, "%c/%s/%s", *field[1], dir, field[1] + 1);
			field[1] = tmp;
		}
		fprintf(out, "%s\t%s\t%s\t%s\n", field[0], field[1], field[2], field[3]);
	}

	err = ferror(in) || ferror(out);

	fclose(in);
	fclose(out);

	return err;
}


static int isdir(char *path, char *file)
{
	char full[PATH_MAX + 1];
	struct stat sbuf;

	snprintf(full, PATH_MAX, "%s/%s", path, file);
	if(stat(full, &sbuf)) {
		perror(full);
		return 0;
	}

	return S_ISDIR(sbuf.st_mode);
}


// If recursing, look for more directories
int read_dir(char *path, int level)
{
	DIR *dir;
	struct dirent *ent;
	int nfiles = 0;
	int len = strlen(path);

	if(!(dir = opendir(path))) {
		perror("opendir");
		return 0;
	}

	while((ent = readdir(dir))) {
		if(*ent->d_name == '.') continue;

		if(isdir(path, ent->d_name)) {
			char *full;

			// note: +2 for / and \0
			if(!(full = malloc(len + strlen(ent->d_name) + 2))) {
				printf("Out of memory\n");
				exit(1);
			}
			if(level == 0)
				strcpy(full, ent->d_name);
			else
				sprintf(full, "%s/%s", path, ent->d_name);
			process_dir(full, level + 1);
			free(full);
		}
	}

	closedir(dir);

	return nfiles;
}

// We do not use mime
void set_mime_file(char *fname) {}

// Dummy functions for config
void set_listen_address(char *addr) {}
void http_set_header(char *fname, int header) {}
