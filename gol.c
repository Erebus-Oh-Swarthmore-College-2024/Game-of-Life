/*
 * Swarthmore College, CS 31
 * Copyright (c) 2021 Swarthmore College Computer Science Department,
 * Swarthmore PA
 */

// Erebus Oh, Jacinta Fernandes-Brough
// CPSC 31, Lab 6

/*
 * To run:
 * ./gol file1.txt  0  # run with config file file1.txt, do not print board
 * ./gol file1.txt  1  # run with config file file1.txt, ascii animation
 * ./gol file1.txt  2  # run with config file file1.txt, ParaVis animation
 *
 */
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   0   // with no animation
#define OUTPUT_ASCII  1   // with ascii animation
#define OUTPUT_VISI   2   // with ParaVis animation

/* Used to slow down animation run modes: usleep(SLEEP_USECS);
 * Change this value to make the animation run faster or slower
 */
//#define SLEEP_USECS  1000000
#define SLEEP_USECS    100000

/* A global variable to keep track of the number of live cells in the
 * world (this is the ONLY global variable you may use in your program)
 */
static int total_live = 0;

/* This struct represents all the data you need to keep track of your GOL
 * simulation.  Rather than passing individual arguments into each function,
 * we'll pass in everything in just one of these structs.
 * this is passed to play_gol, the main gol playing loop
 *
 * NOTE: You will need to use the provided fields here, but you'll also
 *       need to add additional fields. (note the nice field comments!)
 * NOTE: DO NOT CHANGE THE NAME OF THIS STRUCT!!!!
 */
struct gol_data {

   // NOTE: DO NOT CHANGE the names of these 4 fields (but USE them)
   int rows;  // the row dimension
   int cols;  // the column dimension
   int iters; // number of iterations to run the gol simulation
   int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI

  int *board;
   /* fields used by ParaVis library (when run in OUTPUT_VISI mode). */
   // NOTE: DO NOT CHANGE their definitions BUT USE these fields
   visi_handle handle;
   color3 *image_buff;
};

/****************** Function Prototypes **********************/
/* the main gol game playing loop (prototype must match this) */
void play_gol(struct gol_data *data);
/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char *argv[]);

/* print board to the terminal (for OUTPUT_ASCII mode) */
void print_board(struct gol_data *data, int round);

// Given gol_data, returns a separate copy of the board
int *getCopyBoard(struct gol_data *data);
// Given gol_data, and coordinates, returns number of live
// neighbors coordinate has
int numNeighbors(int* b, int x, int y, int rows, int cols);
// calculate neighbor coordinates with torus
int getTorusCoords(int x, int max);
// updates total_live global variable
void recalculateNumAlive(struct gol_data *data);
// update colors for ParVisi to show board
void update_colors(struct gol_data *data);
// function to read 1 int into variable
void readOneInt(FILE *infile, int *a);
// sets up board on init
void initializeBoard(struct gol_data *data, FILE *infile);
/************ Definitions for using ParVisi library ***********/
/* register animation with Paravisi library (DO NOT MODIFY) */
int connect_animation(void (*applfunc)(struct gol_data *data),
    struct gol_data* data);
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";

/**********************************************************/
int main(int argc, char *argv[]) {

  int ret;
  struct gol_data data;
  double secs, startT, stopT;
  struct timeval start_time, stop_time;

  /* check number of command line arguments */
  if (argc < 3) {
    printf("usage: ./gol <infile> <0|1|2>\n");
    printf("(0: with no visi, 1: with ascii visi, 2: with ParaVis visi)\n");
    exit(1);
  }

  /* Initialize game state (all fields in data) from information
   * read from input file */
  ret = init_game_data_from_args(&data, argv);
  if(ret != 0) {
    printf("Error init'ing with file %s, mode %s\n", argv[1], argv[2]);
    exit(1);
  }

  ret = gettimeofday(&start_time, NULL);

  /* Invoke play_gol in different ways based on the run mode */
  if(data.output_mode == OUTPUT_NONE) {  // run with no animation
    play_gol(&data);
  }
  else if (data.output_mode == OUTPUT_ASCII) { // run with ascii animation
    play_gol(&data);

    // clear the previous print_board output from the terminal:
    // (NOTE: you can comment out this line while debugging)
    if(system("clear")) { perror("clear"); exit(1); }

    // NOTE: DO NOT modify this call to print_board at the end
    //       (its for grading)
    print_board(&data, data.iters);
  }
  else {  // OUTPUT_VISI: run with ParaVisi animation
    // call init_pthread_animation and get visi_handle
    data.handle = init_pthread_animation(1, data.rows, data.cols, visi_name);
    if(data.handle == NULL) {
      printf("ERROR init_pthread_animation\n");
      exit(1);
    }
    // get the animation buffer
    data.image_buff = get_animation_buffer(data.handle);
    if(data.image_buff == NULL) {
      printf("ERROR get_animation_buffer returned NULL\n");
      exit(1);
    }
    // connect ParaVisi animation to main application loop function
    connect_animation(play_gol, &data);
    // start ParaVisi animation
    run_animation(data.handle, data.iters);
  }

  ret = gettimeofday(&stop_time, NULL);

  if (data.output_mode != OUTPUT_VISI) {
    startT = (start_time.tv_sec + (start_time.tv_usec / 1000000.0));
    stopT = (stop_time.tv_sec + (stop_time.tv_usec / 1000000.0));
    //printf("%f %f \n", startT, stopT);//
    secs = stopT - startT;

    /* Print the total runtime, in seconds. */
    // NOTE: do not modify these calls to fprintf
    fprintf(stdout, "Total time: %0.3f seconds\n", secs);
    fprintf(stdout, "Number of live cells after %d rounds: %d\n\n",
        data.iters, total_live);
  }

  free(data.board);
  return 0;
}
/**********************************************************/
/* Given a file and gol_data, initializes gol_data's board
 *
 * Helper function for init game.
 */
void initializeBoard(struct gol_data *data, FILE *infile){
  int i, j, ret, r, c, numAlive, a, b;

  readOneInt(infile, &numAlive);
  r = data->rows;
  c = data->cols;
  
  //initialize board to all 0s
  data->board = malloc(r*c*sizeof(int));

  for(i = 0; i < r; i++){
    for(j = 0; j < c; j++){
      data->board[i*c+j] = 0;
    }
  }
  ret = 1;
  //read in alive people coordinates
  if(ret == 1){
    for(i = 0; i < numAlive; i++){
      ret = fscanf(infile, "%d%d", &a, &b);
      data->board[a*c+b] = 1;
    }
  }else{
    printf("Error: reading in live cell coordinates.\n");
    exit(1);
  }
}
/**********************************************************/
/* Given a file and variable address, reads 1 integer and
 * puts it in the variable.
 *
 * Helper function for init game.
 */
void readOneInt(FILE *infile, int *a){
  int ret;
  ret = fscanf(infile, "%d", a);
  if(ret != 1){
    printf("Error: Reading in values.");
    exit(1);
  }
}
/**********************************************************/
/* initialize the gol game state from command line arguments
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * returns: 0 on success, 1 on error
 */
int init_game_data_from_args(struct gol_data *data, char *argv[]) {

  data->output_mode = atoi(argv[2]);

  FILE *infile;
  int r, c, iter;

  //check that we can open file properly
  infile = fopen(argv[1], "r");
  if (infile == NULL){
    printf("Error: file open %s\n", argv[1]);
    exit(1);
  }
  //read in row, col, number of iterations, and number of coordinate
  readOneInt(infile, &r);
  data->rows = r;
  readOneInt(infile, &c);
  data->cols = c;
  readOneInt(infile, &iter);
  data->iters = iter;

  initializeBoard(data, infile);

  recalculateNumAlive(data);
  
  fclose(infile);

  return 0;
}
/**********************************************************/
/* Given gol_data, return a board (dynamic array of
 * row arrays)
 *
 * Helper function to play_gol function
 */
int *getCopyBoard(struct gol_data *data){
  int *boardCopy, i, j;
  // initialize board copy as dynamically allocated array
  boardCopy = malloc(data->cols*data->rows*sizeof(int));

  // copy values from data
  for(i = 0; i < data->rows; i++){
    for(j = 0; j < data->cols; j++){
      boardCopy[i*data->cols+j] = data->board[i*data->cols+j];
    }
  }

  return boardCopy;
}
/**********************************************************/
/* Given a coordinate and maximum, return the proper torus
 * coordinate as if was linked 1D list with lenth of max
 *
 * Helper function for numNeighbors.
 */
int getTorusCoords(int coord, int max){
  if(coord<0){
    return (coord%max)+max;
  }else if(coord >= max){
    return coord%max;
  }else{
    return coord;
  }
}
/**********************************************************/
/* Given gol_data and a coordinate, return the number of
 * live neighbors
 *
 * Helper function for play_gol
 */
int numNeighbors(int* b, int x, int y, int rows, int cols){
  int aroundNums[3], i, j, tempX, tempY, total;
  aroundNums[0] = -1;
  aroundNums[1] = 0;
  aroundNums[2] = 1;
  total = 0;

  for(i = 0; i < 3; i++){
    for(j = 0; j < 3; j++){
      if(aroundNums[i] == 0 && aroundNums[j] == 0){
        continue;
      }else{
        tempX = getTorusCoords(x + aroundNums[i], rows);
        tempY = getTorusCoords(y + aroundNums[j], cols);
        //printf("Looking at (%d,%d)\n", tempX, tempY);
        if(b[tempX*rows+tempY] == 1){
          total++;
        }
      }
    }
  }

  return total;
}
/**********************************************************/
/* Given gol_data and a coordinate, update total_live
 *
 * Helper function for play_gol
 */
void recalculateNumAlive(struct gol_data *data){
  int total, i, j;
  total = 0;

  for(i=0; i < data->rows; i++){
    for(j=0; j < data->cols; j++){
      if(data->board[i*data->cols+j] == 1){
        total++;
      }
    }
  }
  total_live = total;
}
/**********************************************************/
/* updates the colors */
void update_colors(struct gol_data *data) {
  int i, j, r, c, buff_i, index;
  color3 *buff;

  buff = data->image_buff;
  r = data->rows;
  c = data->cols;

  for(i=0; i<r; i++) {
    for (j=0; j<c; j++) {
      index = i*c+j;
      buff_i = (r - (i+1))*c + j;
      if (data->board[index] == 1) {
        buff[buff_i].r = 179;
        buff[buff_i].g = 102;
        buff[buff_i].b = 255;
      }
      else {
        buff[buff_i].r = 0;
        buff[buff_i].g = 0;
        buff[buff_i].b = 0;
      }
    }
  }
}
/**********************************************************/
/* the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *
 *   data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 */
void play_gol(struct gol_data *data) {

  int roundNum, i , j, neighbors, current;
  int *boardCopy;

  boardCopy = NULL;
  // repeat running each round until number of iterations reached
  for(roundNum = 0; roundNum < data->iters; roundNum++){
    // make a copy of current board
    boardCopy = getCopyBoard(data);
    // for each cell of copy board
    for(i = 0; i < data->rows; i++){
      for(j =0; j < data->cols; j++){
        // check number of neighbors
        neighbors = numNeighbors(boardCopy, i, j, data->rows, data->cols);
        // update cell accordingly on original board
        current = boardCopy[i*data->cols+j];
        if(current == 1){
          if(neighbors <= 1){
            data->board[i*data->cols+j] = 0;
          }else if(neighbors >= 4){
            data->board[i*data->cols+j] = 0;
          }
        }else if(current == 0 && neighbors == 3){
          data->board[i*data->cols+j] = 1;
        }
      }
    }

    //free board copy
    free(boardCopy);

    // update total_live
    recalculateNumAlive(data);
    //print_board(data, roundNum+1);

    if (data->output_mode == OUTPUT_ASCII){
      system("clear");
      print_board(data, roundNum+1);
      usleep(SLEEP_USECS);
    }
    else if (data->output_mode == OUTPUT_VISI){
      update_colors(data);
      draw_ready(data->handle);
      usleep(SLEEP_USECS);
    }
  }
}

/**********************************************************/
/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 *
 */
void print_board(struct gol_data *data, int round) {

  int i, j, r, c;

  /* Print the round number. */
  fprintf(stderr, "Round: %d\n", round);

  r = data->rows;
  c = data->cols;

  for (i = 0; i < r; ++i) {
    for (j = 0; j < c; ++j) {
      //printf("data->board[i][j] = %d\n",data->board[i][j]);
      if(data->board[i*c+j] == 1){
        //printf("here\n");
        fprintf(stderr, " @");
      }else{
        fprintf(stderr, " .");
      }
    }
    fprintf(stderr, "\n");
  }
  recalculateNumAlive(data);
    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}


/**********************************************************/
/***** START: DO NOT MODIFY THIS CODE *****/
/* sequential wrapper functions around ParaVis library functions */
void (*mainloop)(struct gol_data *data);

void* seq_do_something(void * args){
  mainloop((struct gol_data *)args);
  return 0;
}

int connect_animation(void (*applfunc)(struct gol_data *data),
    struct gol_data* data)
{
  pthread_t pid;

  mainloop = applfunc;
  if( pthread_create(&pid, NULL, seq_do_something, (void *)data) ) {
    printf("pthread_created failed\n");
    return 1;
  }
  return 0;
}
/***** END: DO NOT MODIFY THIS CODE *****/
/******************************************************/
