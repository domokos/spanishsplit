/*
 * UTILS.h
 *
 */

#ifndef _UTILS_H
#define _UTILS_H

void printUsage(char *command);
int getArgument(int argc, char *argv[],  char** filePrefix, int *nr_of_segments);
int HHMMSS2sec(char* string);
int isHHMMSS(char* string);

#endif
