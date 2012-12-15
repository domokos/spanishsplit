/*
 utils.c

 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void printUsage(char *command)
{
  printf ("Usage:\n %s <input file name> [start stop filename]...\n\n This executable requires 1+2*n arguments:\n <input file name> : process the input file and write the output in a file with the same prefix .raw extension\n [start stop filename] : If start and stop pairs are present then multiple output files are generated, input is split as per the specified segments and named as filename.mp3\n\n",command);
  exit (-1);

}

int HHMMSS2sec(char* string)
{
  return (string[0]-'0')*36000+(string[1]-'0')*3600+(string[2]-'0')*600+(string[3]-'0')*60+(string[4]-'0')*10+(string[5]-'0');
}

int isHHMMSS(char* string)
{
  int i;
  for (i=0; i<strlen(string); i++)
    {
      if (i<2 || i==3 || i==5 )
        {
          if ( !(string[i]>='0' && string[i]<='9')) return 0;
        }else{
          if ( !(string[i]>='0' && string[i]<='5')) return 0;
        }
    }
  return 1;
}

int getArgument(int argc, char *argv[], char** filePrefix, int *nr_of_segments)
{

  /* Check te number of arguments> it must be 3*n + 2 *including executable_name */
  if (argc % 3 != 2) {
      printUsage(argv[0]);
      exit (-1);
  }

  /* get the input file prefix */
  int i = strlen(argv[1])-1;
  int pos = 0;
  while (pos==0) {
      if (argv[1][i]=='.') {
          pos = i;
      }
      i--;
      if (i==0) {
          printf("%s - Error input file  %s doesn't contain any ., impossible to extract prefix\n", argv[0], argv[1]);
          exit(-1);
      }
  }
  *filePrefix = malloc((pos+3)*sizeof(char));
  strncpy(*filePrefix, argv[1], pos);
  (*filePrefix)[pos]='\0';

  *nr_of_segments = (argc -2)/3;

  for(i=0; i<*nr_of_segments; i++)
    {
      if ( (strlen(argv[i*3+2]) != 6) || !isHHMMSS(argv[i*3+2]) )
        {
          printf("%s - Error start parameter %s doesn't match format requirement: HHMMSS\n", argv[0], argv[i*3+2]);
          exit(-1);
        }
      if ( (strlen(argv[i*3+3]) != 6) || !isHHMMSS(argv[i*3+3]) )
        {
          printf("%s - Error stop parameter %s doesn't match format requirement: HHMMSS\n", argv[0], argv[i*3+3]);
          exit(-1);
        }
      if ( HHMMSS2sec(argv[i*3+2]) > HHMMSS2sec(argv[i*3+3]))
        {
          printf("%s - Error start parameter %s is bigger than stop parameter %s\n", argv[0], argv[i*3+2], argv[i*3+3]);
          exit(-1);
        }
    }

  return 0;
}
