#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "database.h"
#include "filesystem.h"

// initialization
// 	needs to be run before using any other functions
int init();

// put file to the filesystem
void putFile(char *filename);

// get file from the filesystem
void getFile(char *filename, char *outfile);