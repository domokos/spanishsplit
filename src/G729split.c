/*
 ============================================================================
 Name        : G729split.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <time.h>


#include "utils.h"
#include "typedef.h"

#include "bcg729/decoder.h"

#define BEFORE_CHUNK 0
#define INSIDE_CHUNK 1
#define AFTER_CHUNK 2


int main(int argc, char *argv[]) {

  /*** get calling argument ***/
  char *filePrefix;
  int nr_of_segments;

  /*
   * check argument, set filePrefix and number
   * of requested segments
   */
  getArgument(argc, argv, &filePrefix, &nr_of_segments);

  /*** input and output file pointers ***/
  FILE *fpInput;
  FILE *fpOutput;

  /*** input and output buffers ***/
  uint8_t bitStream[10]; /* binary input for the decoder */
  int16_t outputBuffer[L_FRAME]; /* output buffer: the reconstructed signal */

  /*** inits ***/
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

  /*** init of the bloc ***/
  bcg729DecoderChannelContextStruct *decoderChannelContext = initBcg729DecoderChannel();


  /* system command buffer for the mp3 conversion*/
  char *command = malloc(sizeof(char)*200);

  /*** initialisation complete ***/


  /* discard vaw header
   * read until string "data" is found
   */
  int data_found = 0;
  char read_char;
  while(!data_found)
    {
      if ((read_char = fgetc(fpInput)) != 'd') continue; else
        {
          if (read_char == EOF)
            {
            printf("%s - Error: no data tag found in wav file %s\n", argv[0], argv[1]);
            exit(-1);
            }
        }
      if (fgetc(fpInput) != 'a') continue;
      if (fgetc(fpInput) != 't') continue;
      if (fgetc(fpInput) != 'a') continue; else data_found = 1;
    }

  /* read and get rid of 4 bytes stSubchunk2Size */
  fread(bitStream, 4,1,fpInput);


  /* 1 frame is 80 output samples. Output sample arte is 8000 Hz */
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

  /* Initialize the state of all chunks and open files respectively
   * 1 sec is 100 frames
   */
  int i;
  for (i=0; i<nr_of_segments; i++)
    {
      chunk_state[i] = BEFORE_CHUNK;
      chunk_start_frame[i] = HHMMSS2sec(argv[i*3+2]) *100;
      chunk_stop_frame[i] = HHMMSS2sec(argv[i*3+3]) *100;
      if (chunk_stop_frame[i] > last_frame_nr){
        printf("%s - Error: stop frame %d beyond end of file\n", argv[0], i+1);
        exit(-1);
      }
      temp_files[i] = malloc(sizeof(char)*200);
    }

  /* Read a frame: 80 bits in 10 bytes */
  while(fread(bitStream, 10,1,fpInput))
    {
      framesNbr++;

      bcg729Decoder(decoderChannelContext, bitStream, 0, outputBuffer);

      for (i=0; i<nr_of_segments; i++)
        {
          switch (chunk_state[i])
          {
          case BEFORE_CHUNK:
            if(framesNbr >= chunk_start_frame[i])
              {
                sprintf(temp_files[i],"%s_temp",argv[i*3+4]);
                if ( (fpChunks[i] = fopen(temp_files[i], "w")) == NULL) {
                    printf("%s - Error: can't create chunk output file  %s\n", argv[0], temp_files[i]);
                    exit(-1);
                }
                fwrite(outputBuffer, sizeof(int16_t), L_FRAME, fpChunks[i]);
                chunk_state[i]=INSIDE_CHUNK;
              }
            break;
          case INSIDE_CHUNK:
            if(framesNbr <= chunk_stop_frame[i])
              {
                fwrite(outputBuffer, sizeof(int16_t), L_FRAME, fpChunks[i]);
              } else{
                fclose(fpChunks[i]);
                chunk_state[i]=AFTER_CHUNK;

                /* convert to mp3 */
                sprintf(command,"lame -r -m m -s 8 --bitwidth 16 %s %s.mp3",temp_files[i],argv[i*3+4]);
                system(command);

                /* convert to mp3 */
                sprintf(command,"rm %s",temp_files[i]);
                system(command);

              }
            break;
          case AFTER_CHUNK:

            break;
          }

        }

      /* write the ouput to raw data file */
      fwrite(outputBuffer, sizeof(int16_t), L_FRAME, fpOutput);

    }

  closeBcg729DecoderChannel(decoderChannelContext);
  fclose(fpOutput);
  fclose(fpInput);

  return EXIT_SUCCESS;
}
