/*
 ============================================================================
 Name        : G729split.c
 Author      : 
 Version     :
 Copyright   :
 Description : G729Split
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <time.h>

#include "typedef.h"

#include "bcg729/decoder.h"

#define BEFORE_CHUNK 0
#define INSIDE_CHUNK 1
#define AFTER_CHUNK 2


/* util functions */

void printUsage(char *command)
{
  printf ("G729 wav input to mp3 converter and splitter.\n\nUsage:\n %s <input file name> [start stop filename]...\n\n This executable converts G729 wav input to mp3. It requires 1+2*n arguments:\n <input file name> : process the input file and write the output in a file with the same prefix adding .mp3 extension\n [start stop filename] : If start and stop pairs are present then multiple output files are generated, input is split as per the specified segments and named as filename\n\n",command);
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
      if ( HHMMSS2sec(argv[i*3+2]) >= HHMMSS2sec(argv[i*3+3]))
        {
          printf("%s - Error start parameter %s is not earlier than stop parameter %s\n", argv[0], argv[i*3+2], argv[i*3+3]);
          exit(-1);
        }
    }

  return 0;
}

/* clear all output and temp files wher error in the middle of processing input */
void cleanup(char **temp_files, int nr_of_segments, char *outputFile)
{
  char command[200];
  int i;
  for(i=0; i<nr_of_segments; i++)
    {
      /* remove segment raw temp files*/
      sprintf(command,"rm -f %s",temp_files[i]);
      system(command);

      /* remove segment mp3 files*/
      sprintf(command,"rm -f %s.mp3",temp_files[i]);
      system(command);
    }

  /* remove main raw temp file*/
  sprintf(command,"rm -f %s",outputFile);
  system(command);
}

/* end of utils block */

int main(int argc, char *argv[]) {

  /* get calling argument */
  char *filePrefix;
  int nr_of_segments;

  /*
   * check argument, set filePrefix and number
   * of requested segments
   */
  getArgument(argc, argv, &filePrefix, &nr_of_segments);

  /* input and output file pointers */
  FILE *fpInput;
  FILE *fpOutput;

  /* input and output buffers */
  uint8_t bitStream[10]; /* binary input for the decoder */
  int16_t outputBuffer[L_FRAME]; /* output buffer: the reconstructed signal */

  /* inits */
  /* open the input file */
  if ( (fpInput = fopen(argv[1], "r")) == NULL) {
          printf("%s - Error: can't open file  %s\n", argv[0], argv[1]);
          exit(-1);
  }

  /* create the output file(filename is the same than input file with the .out extension) */
  char *outputFile = malloc((strlen(filePrefix)+5)*sizeof(char));
  sprintf(outputFile, "%s.raw",filePrefix);
  if ( (fpOutput = fopen(outputFile, "w")) == NULL) {
          printf("%s - Error: can't create file  %s\n", argv[0], outputFile);
          exit(-1);
  }

  /* init of the bloc */
  bcg729DecoderChannelContextStruct *decoderChannelContext = initBcg729DecoderChannel();


  /* system command buffer for the mp3 conversion*/
  char command[200];

  /* initialisation complete */


  /* discard vaw header read until string "data" is found */
  int data_found = 0;
  char read_char;
  while(!data_found)
    {
      read_char = fgetc(fpInput);
      if (read_char  != 'd' && read_char != EOF) continue;
      else if (read_char == EOF)
        {
          printf("%s - Error: no data tag found in wav file %s\n", argv[0], argv[1]);
          exit(-1);
        }
      if (fgetc(fpInput) != 'a') continue;
      if (fgetc(fpInput) != 't') continue;
      if (fgetc(fpInput) != 'a') continue; else data_found = 1;
    }

  /* read and get rid of 4 bytes stSubchunk2Size */
  fread(bitStream, 4,1,fpInput);


  /* 1 frame is 80 output samples. Output sample rate is 8000 Hz */
  int framesNbr =0;
  int last_frame_nr;
  int *chunk_state = malloc(sizeof(int)*nr_of_segments);
  int *chunk_start_frame = malloc(sizeof(int)*nr_of_segments);
  int *chunk_stop_frame = malloc(sizeof(int)*nr_of_segments);
  char *temp_files[200];

  FILE **fpChunks = malloc(sizeof(FILE)*nr_of_segments);

  int position = ftell(fpInput);
  fseek(fpInput, 0L, SEEK_END);

  last_frame_nr = (ftell(fpInput) - position) / 10;

  fseek(fpInput, position, SEEK_SET);

  /* Initialize the state of all chunks - 1 sec is 100 input frames */
  int i;
  for (i=0; i<nr_of_segments; i++)
    {
      chunk_state[i] = BEFORE_CHUNK;
      chunk_start_frame[i] = HHMMSS2sec(argv[i*3+2]) *100;
      chunk_stop_frame[i] = HHMMSS2sec(argv[i*3+3]) *100;
      if (chunk_stop_frame[i] > last_frame_nr){
        printf("%s - Warning: stop frame %d beyond end of file setting it to end of file\n", argv[0], i+1);
        chunk_stop_frame[i] = last_frame_nr;
      }
      temp_files[i] = malloc(sizeof(char)*200);
    }

  /* Read a frame: 80 bits in 10 bytes */
  while(fread(bitStream, 10,1,fpInput))
    {
      framesNbr++;

      /* decode the actual frame */
      bcg729Decoder(decoderChannelContext, bitStream, 0, outputBuffer);

      /* iterate through the requested output chunks and create/close them as needed */
      for (i=0; i<nr_of_segments; i++)
        {
          switch (chunk_state[i])
          {
          /* the chunk's start frame is not yet reached: wait for it or start decoding and change state */
          case BEFORE_CHUNK:
            if(framesNbr >= chunk_start_frame[i])
              {
                sprintf(temp_files[i],"%s_temp",argv[i*3+4]);
                if ( (fpChunks[i] = fopen(temp_files[i], "w")) == NULL) {
                    printf("%s - Error: can't create chunk output file  %s\n", argv[0], temp_files[i]);
                    cleanup(temp_files, nr_of_segments, outputFile);
                    exit(-1);
                }
                /* write the ouput to raw temp data file */
                fwrite(outputBuffer, sizeof(int16_t), L_FRAME, fpChunks[i]);
                chunk_state[i]=INSIDE_CHUNK;
              }
            break;
          /* the chunk is being captured. wait for the ending frame if there close, convert and cleanup */
          case INSIDE_CHUNK:
            if(framesNbr < chunk_stop_frame[i])
              {
                fwrite(outputBuffer, sizeof(int16_t), L_FRAME, fpChunks[i]);
              } else{
                fwrite(outputBuffer, sizeof(int16_t), L_FRAME, fpChunks[i]);
                fclose(fpChunks[i]);
                chunk_state[i]=AFTER_CHUNK;

                /* convert segment to mp3 */
                sprintf(command,"lame -r -m m -s 8 --bitwidth 16 %s %s",temp_files[i],argv[i*3+4]);
                system(command);

                /* remove segment raw temp file*/
                sprintf(command,"rm -f %s",temp_files[i]);
                system(command);

              }
            break;
          case AFTER_CHUNK:
            /* we do not need to do anything here for now */
            break;
          }

        }
      /* write the ouput to raw data file */
      fwrite(outputBuffer, sizeof(int16_t), L_FRAME, fpOutput);
    }

  closeBcg729DecoderChannel(decoderChannelContext);
  fclose(fpOutput);
  fclose(fpInput);

  /* convert main file to mp3 maybe the command could go to a config file.*/
  sprintf(command,"lame -r -m m -s 8 --bitwidth 16 %s %s.mp3",outputFile,filePrefix);
  system(command);

  /* remove main raw temp file*/
  sprintf(command,"rm -f %s",outputFile);
  system(command);

  return EXIT_SUCCESS;
}
