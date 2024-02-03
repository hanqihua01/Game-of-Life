/***********************

Conway's Game of Life

Based on https://web.cs.dal.ca/~arc/teaching/CS4125/2014winter/Assignment2/Assignment2.html

************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

typedef struct
{
    int rows, cols;
    int **cells;
} world;

/* keep short history since we want to detect simple cycles */
#define HISTORY 3

static world worlds[HISTORY];
static int world_iter = 0;
static int world_rows, world_cols; // 行数和列数

static world *cur_world;

static int print_cells = 0; // 每隔多少步打印一次活细胞数
static int print_world = 0; // 每隔多少步打印一次世界

// use fixed world or random world?
#ifdef FIXED_WORLD
static int random_world = 0;
#else
static int random_world = 1;
#endif

static char *start_world[] = {
    /* Gosper glider gun */
    /* example from https://bitstorm.org/gameoflife/ */
    "..........................................",
    "..........................................",
    "..........................................",
    "..........................................",
    "..........................................",
    "..........................................",
    "........................OO.........OO.....",
    ".......................O.O.........OO.....",
    ".OO.......OO...........OO.................",
    ".OO......O.O..............................",
    ".........OO......OO.......................",
    ".................O.O......................",
    ".................O........................",
    "....................................OO....",
    "....................................O.O...",
    "....................................O.....",
    "..........................................",
    "..........................................",
    ".........................OOO..............",
    ".........................O................",
    "..........................O...............",
    "..........................................",
};

static void
world_init_fixed(world *world)
{
    int **cells = world->cells;
    int row, col;

    /* use predefined start_world */

    for (row = 1; row <= world->rows; row++)
    {
        for (col = 1; col <= world->cols; col++)
        {
            if ((row <= sizeof(start_world) / sizeof(char *)) &&
                (col <= strlen(start_world[row - 1])))
            {
                cells[row][col] = (start_world[row - 1][col - 1] != '.');
            }
            else
            {
                cells[row][col] = 0;
            }
        }
    }
}

static void
world_init_random(world *world)
{
    int **cells = world->cells;
    int row, col;

    // Note that rand() implementation is platform dependent.
    // At least make it reprodible on this platform by means of srand()
    srand(1);

    for (row = 1; row <= world->rows; row++)
    {
        for (col = 1; col <= world->cols; col++)
        {
            float x = rand() / ((float)RAND_MAX + 1);
            if (x < 0.5)
            {
                cells[row][col] = 0;
            }
            else
            {
                cells[row][col] = 1;
            }
        }
    }
}

static void
world_print(world *world)
{
    int **cells = world->cells;
    int row, col;

    for (row = 1; row <= world->rows; row++)
    {
        for (col = 1; col <= world->cols; col++)
        {
            if (cells[row][col])
            {
                printf("O");
            }
            else
            {
                printf(" ");
            }
        }
        printf("\n");
    }
}

static int
world_count(world *world)
{
    int **cells = world->cells;
    int isum;
    int row, col;

    isum = 0;
    for (row = 1; row <= world->rows; row++)
    {
        for (col = 1; col <= world->cols; col++)
        {
            isum = isum + cells[row][col];
        }
    }

    return isum;
}

/* Take world wrap-around into account: */
static void
world_border_wrap(world *world)
{
    int **cells = world->cells;
    int row, col;

    /* left-right boundary conditions */
    for (row = 1; row <= world->rows; row++)
    {
        cells[row][0] = cells[row][world->cols];
        cells[row][world->cols + 1] = cells[row][1];
    }

    /* top-bottom boundary conditions */
    for (col = 0; col <= world->cols + 1; col++)
    {
        cells[0][col] = cells[world->rows][col];
        cells[world->rows + 1][col] = cells[1][col];
    }
}

// update board for next timestep
// rows/cols params are the base rows/cols
// excluding the surrounding 1-cell wraparound border
static void
world_timestep(world *old, world *new)
{
    int **cells = old->cells;
    int row, col;

    // update board
    for (row = 1; row <= new->rows; row++)
    {
        for (col = 1; col <= new->cols; col++)
        {
            int row_m, row_p, col_m, col_p, nsum;
            int newval;

            // sum surrounding cells
            row_m = row - 1;
            row_p = row + 1;
            col_m = col - 1;
            col_p = col + 1;

            nsum = cells[row_p][col_m] + cells[row_p][col] + cells[row_p][col_p] + cells[row][col_m] + cells[row][col_p] + cells[row_m][col_m] + cells[row_m][col] + cells[row_m][col_p];

            switch (nsum)
            {
            case 3:
                // a new cell is born
                newval = 1;
                break;
            case 2:
                // the cell, if any, survives
                newval = cells[row][col];
                break;
            default:
                // the cell, if any, dies
                newval = 0;
                break;
            }

            new->cells[row][col] = newval;
        }
    }
}

static int
world_check_cycles(world *cur_world, int iter)
{
    int i;

    /* This version re-applies the border wraps so they are consistent with
     * the respective world states, and we can just compare the full world arrays.
     */
    world_border_wrap(cur_world);
    for (i = iter - 1; i >= 0 && i > iter - HISTORY; i--)
    {
        world *prev_world = &worlds[i % HISTORY];

        world_border_wrap(prev_world);
        if (memcmp(&cur_world->cells[0][0], &prev_world->cells[0][0],
                   (world_rows + 2) * (world_cols + 2) * sizeof(int)) == 0)
        {
            printf("world iteration %d is equal to iteration %d\n", iter, i);
            return 1;
        }
    }

    return 0;
}

static int **
alloc_2d_int_array(int nrows, int ncolumns)
{
    int **array;
    int row;

    /* version that keeps the 2d data contiguous, can help caching and slicing across dimensions */
    array = malloc(nrows * sizeof(int *));
    if (array == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    array[0] = malloc(nrows * ncolumns * sizeof(int));
    if (array[0] == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    /* memory layout is row-major */
    for (row = 1; row < nrows; row++)
    {
        array[row] = array[0] + row * ncolumns;
    }

    return array;
}

static double
time_secs(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, 0) != 0)
    {
        fprintf(stderr, "could not do timing\n");
        exit(1);
    }

    return tv.tv_sec + (tv.tv_usec / 1000000.0);
}

int main(int argc, char *argv[])
{
    int h, nsteps;
    double start_time, end_time, elapsed_time;

    /* Get Parameters */
    if (argc != 6)
    {
        fprintf(stderr, "Usage: %s rows cols steps worldstep cellstep\n", argv[0]);
        exit(1);
    }
    world_rows = atoi(argv[1]);
    world_cols = atoi(argv[2]);
    nsteps = atoi(argv[3]);
    print_world = atoi(argv[4]);
    print_cells = atoi(argv[5]);

    /* initialize worlds, when allocating arrays, add 2 for ghost cells in both directorions */
    for (h = 0; h < HISTORY; h++)
    {
        worlds[h].rows = world_rows;
        worlds[h].cols = world_cols;
        worlds[h].cells = alloc_2d_int_array(world_rows + 2, world_cols + 2);
    }

    /*  initialize board */
    cur_world = &worlds[world_iter % HISTORY];
    if (random_world)
    {
        world_init_random(cur_world);
    }
    else
    {
        world_init_fixed(cur_world);
    }

    if (print_world > 0)
    {
        printf("\ninitial world:\n\n");
        world_print(cur_world);
    }

    start_time = time_secs();

    /*  time steps */
    for (world_iter = 1; world_iter < nsteps; world_iter++)
    {
        world *next_world;
        int cycle;

        next_world = &worlds[world_iter % HISTORY];

        world_border_wrap(cur_world);
        world_timestep(cur_world, next_world);
        cur_world = next_world;

        cycle = world_check_cycles(cur_world, world_iter);

        if (print_cells > 0 && (world_iter % print_cells) == (print_cells - 1))
        {
            printf("%d: %d live cells\n", world_iter, world_count(cur_world));
        }

        if (print_world > 0 && (cycle || (world_iter % print_world) == (print_world - 1)))
        {
            printf("\nat time step %d:\n\n", world_iter);
            world_print(cur_world);
        }

        if (cycle)
        {
            break;
        }
    }

    end_time = time_secs();
    elapsed_time = end_time - start_time;
    
    /*  Iterations are done; sum the number of live cells */
    printf("Number of live cells = %d\n", world_count(cur_world));
    fprintf(stderr, "Game of Life took %10.3f seconds\n", elapsed_time);

    return 0;
}
