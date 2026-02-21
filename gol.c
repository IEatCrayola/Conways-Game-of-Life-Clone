/*
 * Swarthmore College, CS 31
 * Copyright (c) 2023 Swarthmore College Computer Science Department,
 * Swarthmore PA
 */

/*
* This program runs the game of life game using multiple threads.
* Using multiple threads, parrallelization, and synchroniztion
* increases the speed at which the program runs and makes it
* more efficient. After implementation, users are able to
* call the program and uses different thread implementation.
 */

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
#include "colors.h"

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   (0)   // with no animation
#define OUTPUT_ASCII  (1)   // with ascii animation
#define OUTPUT_VISI   (2)   // with ParaVis animation

/* Used to slow down animation run modes: usleep(SLEEP_USECS);
 * Change this value to make the animation run faster or slower
 */
//#define SLEEP_USECS  (1000000)
#define SLEEP_USECS    (200000)

/* A global variable to keep track of the number of live cells in the
 * world (this is the ONLY global variable you may use in your program)
 */
static int total_live = 0;

/* declare a mutex: initialize in main */
static pthread_mutex_t my_mutex;

/* declare a barrier: initialize in main */
static pthread_barrier_t my_barrier;

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

    int  liveCells;

    int* currentBoard;
    int* nextBoard;
    int curr_iter;

    int id;
    int control;
    int toPrint;
    int num_threads;
    int start_row;
    int end_row;
    int start_col;
    int end_col;

    /* fields used by ParaVis library (when run in OUTPUT_VISI mode). */
    // NOTE: DO NOT CHANGE their definitions BUT USE these fields
    visi_handle handle;
    color3 *image_buff;
};


/****************** Function Prototypes **********************/

/* the main gol game playing loop (prototype must match this) */
void *play_gol(void* args);

/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char **argv);

// A mostly implemented function, but a bit more for you to add.
/* print board to the terminal (for OUTPUT_ASCII mode) */
void print_board(struct gol_data *data, int round);

int inBounds(struct gol_data *data, int i, int j, int iDisp, int jDisp);

int findCoord(struct gol_data *data, int val, int rowOrCol);

void simulation(struct gol_data *data);

void update_colors(struct gol_data *data);

void partition(struct gol_data *data);

/**************************************************************/


/************ Definitions for using ParVisi library ***********/
/* initialization for the ParaVisi library (DO NOT MODIFY) */
int setup_animation(struct gol_data* data);
/* register animation with ParaVisi library (DO NOT MODIFY) */
int connect_animation(void (*applfunc)(struct gol_data *data),
        struct gol_data* data);
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";
/**************************************************************/


/************************ Main Function ***********************/
int main(int argc, char **argv) {

    int ret;
    struct gol_data data;
    struct gol_data *targs;
    double secs;

    pthread_t *tid;

    /* check number of command line arguments */
    if (argc < 6) {
        printf("usage: %s infile.txt output_mode[0,1,2] num_threads partition[0,1] print_partition[0,1]\n", argv[0]);
        printf("(0: no visualization, 1: ASCII, 2: ParaVisi)\n");
        exit(1);
    }

    if (atoi(argv[4]) != 0 && atoi(argv[4]) != 1) {
        printf("Invalid partitioning control input\n");
        exit(1);
    }

    if (atoi(argv[5]) != 0 && atoi(argv[5]) != 1) {
        printf("Invalid print control input\n");
        exit(1);
    }
    

    /* Initialize game state (all fields in data) from information
     * read from input file */
    ret = init_game_data_from_args(&data, argv);
    if (ret != 0) {
        printf("Initialization error: file %s, mode %s\n", argv[1], argv[2]);
        exit(1);
    }

    if (atoi(argv[3]) < 1 || atoi(argv[3]) > data.rows || atoi(argv[3]) > data.cols) {
        printf("Incorrect number of threads\n");
        exit(1);
    }

    data.num_threads = atoi(argv[3]);
    data.control = atoi(argv[4]);
    data.toPrint = atoi(argv[5]);

    tid = malloc(sizeof(pthread_t) * data.num_threads);
    if (!tid) { perror("malloc: pthread_t array"); exit(1); }
    
    targs = malloc(sizeof(struct gol_data) * data.num_threads);
    if (!targs) { perror("malloc: targs array"); exit(1); }

    // Initialize the mutex
    if (pthread_mutex_init(&my_mutex, NULL)) { 
        printf("pthread_mutex_init error\n");
        exit(1);
    }
    
    // Initialize the barrier with num threads that will be synchronized
    if (pthread_barrier_init(&my_barrier, NULL, data.num_threads)) {
        printf("pthread_barrier_init error\n");
        exit(1);
    }

    if (data.output_mode == OUTPUT_VISI) {
        setup_animation(&data);
    }


    for (int i = 0; i < data.num_threads; i++) {
        targs[i] = data;
        targs[i].id = i;

        ret = pthread_create(&tid[i], 0, play_gol, &targs[i]);
        if (ret) { perror("Error pthread_create\n"); exit(1); }
    }

    
    if (data.output_mode == OUTPUT_VISI) {
        run_animation(data.handle, data.iters);
    }

    for (int i = 0; i < data.num_threads; i++) {
        pthread_join(tid[i], 0);
      }

free(data.currentBoard);
free(data.nextBoard);
free(tid);
free(targs);
    
    data.currentBoard = NULL;
    data.nextBoard = NULL;
    targs = NULL;
    tid = NULL;
  

    /* Release the synchronization variables. */
    if (pthread_mutex_destroy(&my_mutex)) {
        printf("pthread_mutex_destroy error\n");
        exit(1);
    }
    
    if (pthread_barrier_destroy(&my_barrier)) {
        printf("pthread_barrier_destroy error\n");
        exit(1);
    }

    return 0;
}
/**************************************************************/


/******************** Function Prototypes ************************/

/**************************************************************/
//
//       As always, add your own additional helper function(s)
//       for implementing partial functionality of these big
//       parts of the application, apply good top-down design
//       in your solution, and use incremental implementation
//       and testing as you go.

/* initialize the gol game state from command line arguments
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * returns: 0 on success, 1 on error
 */
int init_game_data_from_args(struct gol_data *data, char **argv) {

    int ret;
    int i;

    data->output_mode = atoi(argv[2]);

    FILE *infile;
    infile = fopen(argv[1], "r");

    if (infile == NULL) {
        printf("Error: failed to open file: %s\n", argv[1]);
        exit(1);
    }

    ret = fscanf(infile, "%d", &data->rows);
    if (ret != 1) {
        printf("Improper file format.\n");
        exit(1);
    }

    ret = fscanf(infile, "%d", &data->cols);
    if (ret != 1) {
        printf("Improper file format.\n");
        exit(1);
    }

    ret = fscanf(infile, "%d", &data->iters);
    if (ret != 1) {
        printf("Improper file format.\n");
        exit(1);
    }

    ret = fscanf(infile, "%d", &data->liveCells);
    if (ret != 1) {
        printf("Improper file format.\n");
        exit(1);
    }
    
    data->currentBoard = malloc((data->rows * data->cols) * sizeof(int));
    data->nextBoard = malloc((data->rows * data->cols) * sizeof(int));

    for (i = 0; i < data->rows * data->cols; i++) {
        data->currentBoard[i] = 0;
        data->nextBoard[i] = 0;
    }

    ret = 2;

    i = 0;
    int row;
    int col;

    while ((i < data->liveCells*2) && (ret == 2)) {
        // read in the next line
        ret = fscanf(infile, "%d%d", &row, &col);
        if (ret != 2) {
            printf("Improper file format.\n");
            exit(1);
        }
        data->currentBoard[row * data->cols + col] = 1;
        i += 2;
    }
    
    ret = fclose(infile);
    if (ret != 0) {
        printf("Error: failed to close file: %s\n", argv[1]);
        exit(1);
    }
   

    return 0;
}
/**************************************************************/

/* the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *
 *   data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 */
void *play_gol(void* args) {
    //  at the end of each round of GOL, determine if there is an
    //  animation step to take based on the output_mode,
    //   if ascii animation:
    //     (a) call system("clear") to clear previous world state from terminal
    //     (b) call print_board function to print current world state
    //     (c) call usleep(SLEEP_USECS) to slow down the animation
    //   if ParaVis animation:
    //     (a) call your function to update the color3 buffer
    //     (b) call draw_ready(data->handle)
    //     (c) call usleep(SLEEP_USECS) to slow down the animation

    struct gol_data *data;

    data = (struct gol_data *)args;
    partition(data);

    int displacement[16] = {1, 0, 0, -1, -1, 0, 0, 1, 1, 1, 1, -1, -1, -1, -1, 1};
    int* temp;
    int currentLive = 0;

    
    for (int len = 1; len <= data->iters; len++) {
        int i, j, newI, newJ;
        currentLive = 0;
        
        for (i = data->start_row; i <= data->end_row; i++) {
            for (j = data->start_col; j <= data->end_col; j++) {
                int live_Neighbors = 0;
                for (int k = 0; k < 16; k += 2) {
                    newI = findCoord(data, i+displacement[k], 0);
                    newJ = findCoord(data, j+displacement[k+1], 1);

                    if (data->currentBoard[newI* data->cols + newJ] == 1) {
                        live_Neighbors += 1;
                    } 
                }
     
                if ((data->currentBoard[i * data->cols + j] == 0 && (live_Neighbors == 3)) || (data->currentBoard[i * data->cols + j] == 1 && (live_Neighbors == 2 || live_Neighbors == 3))) {
                    data->nextBoard[i * data->cols + j] = 1;
                    currentLive++;
                } else {
                    data->nextBoard[i * data->cols + j] = 0;
                }
            }
        }

        
        
        temp = data->currentBoard;
        data->currentBoard = data->nextBoard;
        data->nextBoard = temp;

        if (data->id == 0) {
            total_live = 0;
        }

        pthread_barrier_wait(&my_barrier);

        pthread_barrier_wait(&my_barrier);
        total_live += currentLive;
        pthread_barrier_wait(&my_barrier);

        if (data->id == 0) {
            if (data->output_mode == OUTPUT_NONE) {
                NULL;
            }
            else if(data->output_mode == OUTPUT_ASCII) {
                system("clear");
                print_board(data, len);
                usleep(200000);
            }
        }

        if (data->output_mode == OUTPUT_VISI) {
            update_colors(data);
            draw_ready(data->handle);
            usleep(SLEEP_USECS);
        }

        pthread_barrier_wait(&my_barrier);

    }

    return NULL;
}
/**************************************************************/

/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 *
 * NOTE: You may add extra printfs if you'd like, but please
 *       leave these fprintf calls exactly as they are to make
 *       grading easier!
 */
void print_board(struct gol_data *data, int round) {

    int i, j;

    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", round);
    //fprintf(stderr, "%d\n", data->id);

    for (i = 0; i < data->rows; i++) {
        for (j = 0; j < data->cols; j++) {
            if(data->currentBoard[i * data->cols + j] == 1) {
                fprintf(stderr, " @");
            }
            else {
                fprintf(stderr, " .");
            }
        }
        fprintf(stderr, "\n");
    }

    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}
/**************************************************************/


int inBounds(struct gol_data *data, int i, int j, int iDisp, int jDisp) {
    int newI = i + iDisp;
    int newJ = j + jDisp;

    if ((newI >= 0 && newI < data->cols) && (newJ >= 0 && newJ < data->rows)) {
        return 1;
    }
    
    return 0;
}


/* Row == 0, Col == 1*/
int findCoord(struct gol_data *data, int val, int rowOrCol) {
    int newCoord = val;

    if (rowOrCol == 0) {
        if (val >= data->rows) {
            newCoord = val - data->rows;
        } else if (val < 0) {
            newCoord = data->rows + val;
        }
    }

    if (rowOrCol == 1) {
        if (val >= data->cols) {
            newCoord = val - data->cols;
        } else if (val < 0) {
            newCoord = data->cols + val;
        }
    }

    return newCoord;
}

void simulation(struct gol_data *data) {

    data->curr_iter = 0;
    int iters, i, r, c, index, rows, cols;

    /* for readability */
    iters = data->iters;
    rows = data->rows;
    cols = data->cols;

    for (i = 0; i < iters; i++ ) {  /* run some number of iters */

        /* in each iteration, adjust the grid with these rules */
        for (r = 0; r < rows; r++) {   // for every row...
            for (c = 0; c < cols; c++) {   // and every col...
                index = r*cols + c;
                data->currentBoard[index] = (data->currentBoard[index]);
            }
        }
        data->curr_iter = data->curr_iter + 1;

        /* when we're all done updating our grid, update the visualization */
    }
}

void update_colors(struct gol_data *data) {

    int i, j, r, c, index, buff_i;
    color3 *buff;

    buff = data->image_buff;  // just for readability
    r = data->rows;
    c = data->cols;


    for (i = data->start_row; i <= data->end_row; i++) {
        for (j = data->start_col; j <= data->end_col; j++) {
            index = i*c + j;
            // translate row index to y-coordinate value because in
            // the image buffer, (r,c)=(0,0) is the _lower_ left but
            // in the grid, (r,c)=(0,0) is _upper_ left.
            buff_i = (r - (i+1))*c + j;

            // update animation buffer
            if (data->currentBoard[index] == 1) {
                buff[buff_i] = c3_black;
            } else if (data->currentBoard[index] == 0) {
                buff[buff_i] = colors[(data->id) % 8];
            } 
        }
    }
}


void partition(struct gol_data *data) {
    int rows = data->rows;
    int cols = data->cols;
    int threads = data->num_threads;
    int start = 0;
    int last = 0;
    int control = data->control;

    // row paritioning
    if (control == 0) {
        int remainder = cols % threads;
        for (int i = 0; i < threads; i++) {
            if (i < remainder) {
                last = start + (cols / threads);
            } else {
                last = start +(cols / threads) - 1;
            }
            

            if (i == data->id) {
                if (data->toPrint == 1) {
                    printf("tid %d: rows: %d:%d (%d) cols %d:%d (%d)\n", i, start, last, last-start+1, 0, cols-1, cols);
                    fflush(stdout);
                }
                data->start_row = start;
                data->end_row = last;
                data->start_col = 0;
                data->end_col = data->cols-1;
            }
            start = last + 1;
        }
    } else if (control == 1) {
        int remainder = rows % threads;
        for (int i = 0; i < threads; i++) {
            if (i < remainder) {
                last = start + (rows / threads);
            } else {
                last = start +(rows / threads) - 1;
            }

            if (i == data->id) {
                if (data->toPrint == 1) {
                    printf("tid %d: rows: %d:%d (%d) cols %d:%d (%d)\n", i, 0, rows-1, rows, start, last, last-start+1);
                    fflush(stdout);
                }
                data->start_row = 0;
                data->end_row = data->rows-1;
                data->start_col = start;
                data->end_col = last;
            }

            start = last + 1;
        }
    }
}




/**************************************************************/
/***** START: DO NOT MODIFY THIS CODE *****/
/* initialize ParaVisi animation */
int setup_animation(struct gol_data* data) {
    /* connect handle to the animation */
    int num_threads = data->num_threads;
    data->handle = init_pthread_animation(num_threads, data->rows,
            data->cols, visi_name);
    if (data->handle == NULL) {
        printf("ERROR init_pthread_animation\n");
        exit(1);
    }
    // get the animation buffer
    data->image_buff = get_animation_buffer(data->handle);
    if(data->image_buff == NULL) {
        printf("ERROR get_animation_buffer returned NULL\n");
        exit(1);
    }
    return 0;
}

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
/**************************************************************/



