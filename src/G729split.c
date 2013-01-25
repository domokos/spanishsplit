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

/* globals */
char *rm_binary, *encoder_binary, *encoder_parameters, *output_extension;

/* util functions */

void printUsage(char *command)
{
  printf ("G729 wav input converter and splitter.\n\nUsage:\n %s <input file name> <output extension> <encoder_binary> <encoder_parameters> <rm_binary> [start stop filename]...\n\n This executable converts G729 wav input to raw PCM and then passes it to the <encoder_binary> program. It requires 1+2*n arguments:\n <input file name> : process the input file and write the output in a file with the same prefix adding .<output_extension> extension\n [start stop filename] : If start and stop pairs are present then multiple output files are generated, input is split as per the specified segments and named as filename\n\n",command);
  exit (-1);

}

int HHMMSS2sec(char* string)
{
  return (string[0]-'0')*36000+(string[1]-'0')*3600+(string[2]-'0')*600+(string[3]-'0')*60+(string[4]-'0')*10+(string[5]-'0');
}

int isHHMMSS(char* string)
{
  if (strlen(string) !=6) return 0;

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

  /* Check the number of arguments> it must be 3*n + 6 *including executable_name */
  if (argc % 3 != 0 || argc < 6) {
      printUsage(argv[0]);
      exit (-1);
  }

  /* get the input file prefix */
  int i = strlen(argv[1])-1;
  int endpos = i;
  int pos = 0;
  while (pos==0)
    {
      if (argv[1][i]=='.') {pos = i;}
      if (i<1 || pos == endpos) {
          printf("%s - Error input file: %s doesn't contain any . or extension after. , impossible to extract prefix\n", argv[0], argv[1]);
          exit(-1);
      }
      i--;
  }
  *filePrefix = malloc((pos+3)*sizeof(char));
  strncpy(*filePrefix, argv[1], pos);
  (*filePrefix)[pos]='\0';

  char *base = strrchr((*filePrefix), '/');
  if (base) (*filePrefix) = base+1;


  *nr_of_segments = (argc - 6)/3;

  for(i=0; i<*nr_of_segments; i++)
    {
      if ( !isHHMMSS(argv[i*3+6]) )
        {
          printf("%s - Error start parameter %s doesn't match format requirement: HHMMSS\n", argv[0], argv[i*3+5]);
          exit(-1);
        }
      if ( !isHHMMSS(argv[i*3+7]) )
        {
          printf("%s - Error stop parameter %s doesn't match format requirement: HHMMSS\n", argv[0], argv[i*3+6]);
          exit(-1);
        }
      if ( HHMMSS2sec(argv[i*3+6]) >= HHMMSS2sec(argv[i*3+7]))
        {
          printf("%s - Error start parameter %s is not earlier than stop parameter %s\n", argv[0], argv[i*3+5], argv[i*3+6]);
          exit(-1);
        }
    }

  return 0;
}

/* clear all output and temp files when error in the middle of processing input */
void cleanup(char **temp_files, FILE** temp_filehandles, int nr_of_segments, char *outputFile, FILE* output_filehandle, char **argv)
{
  char command[200];
  int i;
  for(i=0; i<nr_of_segments; i++)
    {
      /* remove segment raw temp files*/
      fclose(temp_filehandles[i]);
      sprintf(command,"%s -f %s",rm_binary, temp_files[i]);
      system(command);

      /* remove segment output files*/
      sprintf(command,"%s -f %s",rm_binary, argv[i*3+8]);
      system(command);
    }

  /* remove main raw temp file*/
  fclose(output_filehandle);
  sprintf(command,"%s -f %s",rm_binary, outputFile);
  system(command);
}

/* end of utils block */

int main(int argc, char *argv[]) {

  char *filePrefix;
  int nr_of_segments;

  encoder_binary = argv[2];
  output_extension = argv[3];
  encoder_parameters = argv[4];
  rm_binary = argv[5];

  /*
   * Get calling arguments, set filePrefix and number
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

  /* create the output file(filename is the same than input file with the .raw extension) */
  char *outputFile = malloc((strlen(filePrefix)+5)*sizeof(char));
  sprintf(outputFile, "%s.raw",filePrefix);
  if ( (fpOutput = fopen(outputFile, "w")) == NULL) {
          printf("%s - Error: can't create file  %s\n", argv[0], outputFile);
          exit(-1);
  }

  /* init of the bloc */
  bcg729DecoderChannelContextStruct *decoderChannelContext = initBcg729DecoderChannel();


  /* system command buffer for the output conversion*/
  char command[400];

  /* initialisation complete */

  /* discard vaw header read until string "data" is found */
  int read_char, data_found = 0, at_eof = 0;
  while(!data_found && !at_eof)
    {
      if ((read_char=fgetc(fpInput)) != 'd')
        {
          at_eof = (read_char == EOF);
          continue;
        }
      if (fgetc(fpInput) != 'a') continue;
      if (fgetc(fpInput) != 't') continue;
      if (fgetc(fpInput) != 'a') continue; else data_found = 1;
    }

  if(at_eof)
  {
    printf("%s - Error: no data tag found in wav file %s\n", argv[0], argv[1]);
    cleanup(NULL,NULL,0,outputFile,fpOutput,argv);
    exit(-1);
  }

  /* read and get rid of 4 bytes stSubchunk2Size */
  if ( fread(bitStream, 4,1,fpInput) != 1)
  {
    printf("%s - Error: no valid WAV after data tag in file: %s\n", argv[0], argv[1]);
    cleanup(NULL,NULL,0,outputFile,fpOutput,argv);
    exit(-1);
  }

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

  if (last_frame_nr == 0)
  {
    printf("%s - Error: no valid voice data in file: %s\n", argv[0], argv[1]);
    cleanup(NULL,NULL,0,outputFile,fpOutput,argv);
    exit(-1);
  }

  /* Initialize the state of all chunks - 1 sec is 100 input frames */
  int i;
  for (i=0; i<nr_of_segments; i++)
    {
      chunk_state[i] = BEFORE_CHUNK;
      chunk_start_frame[i] = HHMMSS2sec(argv[i*3+6]) * 100;
      if (chunk_start_frame[i] >= last_frame_nr){
        printf("%s - Error: start frame %d beyond or at end of file.\n", argv[0], i+1);
        cleanup(NULL,NULL,0,outputFile,fpOutput,argv);
        exit(-1);
      }
      chunk_stop_frame[i] = HHMMSS2sec(argv[i*3+7]) * 100;
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
                sprintf(temp_files[i],"%s_temp",argv[i*3+8]);
                if ( (fpChunks[i] = fopen(temp_files[i], "w")) == NULL) {
                    printf("%s - Error: can't create chunk output file  %s\n", argv[0], temp_files[i]);
                    cleanup(temp_files, fpChunks, nr_of_segments, outputFile,fpOutput,argv);
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

                /* convert segment to output format */
                sprintf(command,"%s %s %s %s",encoder_binary, encoder_parameters, temp_files[i], argv[i*3+8]);
                system(command);

                /* remove segment raw temp file*/
                sprintf(command,"%s -f %s",rm_binary, temp_files[i]);
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

  /* convert main file to output format.*/
  sprintf(command,"%s %s %s %s.%s",encoder_binary, encoder_parameters, outputFile, filePrefix, output_extension);
  system(command);

  /* remove main raw temp file*/
  sprintf(command,"%s -f %s",rm_binary, outputFile);
  system(command);

  return EXIT_SUCCESS;
}
