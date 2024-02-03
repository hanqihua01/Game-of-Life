/***********************

Conway's Game of Life

Based on https://web.cs.dal.ca/~arc/teaching/CS4125/2014winter/Assignment2/Assignment2.html

************************/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

int rank, size;

typedef struct
{
    int rows, cols;
    int **cells;
} world;

// define the type of partial world
typedef struct
{
    int rows, cols;
    int **cells;
} partial_world;

/* keep short history since we want to detect simple cycles */
#define HISTORY 3

static int world_iter = 0;
static int world_rows, world_cols;

static world *cur_world;

static int print_cells = 0;
static int print_world = 0;

static partial_world partial_worlds[HISTORY];      // HISTORY partial worlds
static int partial_world_rows, partial_world_cols; // number of rows and columns of the partial world
static int partial_world_start, partial_world_end; // start and end row of the partial world
static partial_world *cur_partial_world;           // current partial world

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

// fill ghost cells through MPI
static void
partial_world_border_wrap(partial_world *partial_world)
{
    int **cells = partial_world->cells;

    for (int row = 1; row <= partial_world->rows; row++)
    {
        cells[row][0] = cells[row][partial_world->cols];
        cells[row][partial_world->cols + 1] = cells[row][1];
    }

    int target_rank1 = rank - 1;
    int target_rank2 = rank + 1;
    if (target_rank1 < 0)
        target_rank1 = size - 1;
    if (target_rank2 >= size)
        target_rank2 = 0;

    MPI_Request request1, request2, request3, request4;
    MPI_Isend(&cells[1][0], partial_world->cols + 2, MPI_INT, target_rank1, 0, MPI_COMM_WORLD, &request1);
    MPI_Isend(&cells[partial_world->rows][0], partial_world->cols + 2, MPI_INT, target_rank2, 1, MPI_COMM_WORLD, &request2);
    MPI_Irecv(&cells[0][0], partial_world->cols + 2, MPI_INT, target_rank1, 1, MPI_COMM_WORLD, &request3);
    MPI_Irecv(&cells[partial_world->rows + 1][0], partial_world->cols + 2, MPI_INT, target_rank2, 0, MPI_COMM_WORLD, &request4);
    MPI_Wait(&request1, MPI_STATUS_IGNORE);
    MPI_Wait(&request2, MPI_STATUS_IGNORE);
    MPI_Wait(&request3, MPI_STATUS_IGNORE);
    MPI_Wait(&request4, MPI_STATUS_IGNORE);
}

// caculate the next state of the partial world
static void
partial_world_timestep(partial_world *old, partial_world *new)
{
    int **cells = old->cells;

    for (int row = 1; row <= new->rows; ++row)
        for (int col = 1; col <= new->cols; ++col)
        {
            int row_m, row_p, col_m, col_p, nsum;
            int newval;

            row_m = row - 1;
            row_p = row + 1;
            col_m = col - 1;
            col_p = col + 1;

            nsum = cells[row_p][col_m] + cells[row_p][col] + cells[row_p][col_p] + cells[row][col_m] + cells[row][col_p] + cells[row_m][col_m] + cells[row_m][col] + cells[row_m][col_p];

            switch (nsum)
            {
            case 3:
                newval = 1;
                break;
            case 2:
                newval = cells[row][col];
                break;
            default:
                newval = 0;
                break;
            }

            new->cells[row][col] = newval;
        }
}

// check if the partial world is in a cycle
static int
partial_world_check_cycles(partial_world *cur_partial_world, int iter)
{
    for (int i = iter - 1; i >= 0 && i > iter - HISTORY; i--)
    {
        partial_world *prev_partial_world = &partial_worlds[i % HISTORY];

        if (memcmp(&cur_partial_world->cells[0][0], &prev_partial_world->cells[0][0],
                   (partial_world_rows + 2) * (partial_world_cols + 2) * sizeof(int)) == 0)
            return i;
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

// rank0 collects the partial worlds from other ranks
static void
collect_world(void)
{
    if (rank != 0)
        for (int i = 1; i <= partial_world_rows; i++)
            MPI_Send(&cur_partial_world->cells[i][1], partial_world_cols, MPI_INT, 0, i + partial_world_start, MPI_COMM_WORLD);
    else
    {
        for (int i = 1; i <= partial_world_rows; i++)
            for (int j = 1; j <= partial_world_cols; j++)
                cur_world->cells[i + partial_world_start][j] = cur_partial_world->cells[i][j];
        for (int i = 1 + partial_world_rows; i <= world_rows; i++)
            MPI_Recv(&cur_world->cells[i][1], partial_world_cols, MPI_INT, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

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

    // initialize the world
    cur_world = (world *)malloc(sizeof(world));
    cur_world->rows = world_rows;
    cur_world->cols = world_cols;
    cur_world->cells = alloc_2d_int_array(world_rows + 2, world_cols + 2);

    if (rank == 0)
    {
        if (random_world)
            world_init_random(cur_world);
        else
            world_init_fixed(cur_world);

        if (print_world > 0)
        {
            printf("\ninitial world:\n\n");
            world_print(cur_world);
        }
    }

    // broadcast the world to other ranks
    MPI_Bcast(&cur_world->cells[0][0], (world_rows + 2) * (world_cols + 2), MPI_INT, 0, MPI_COMM_WORLD);

    // initialize the partial world
    if (world_rows % size == 0)
    {
        partial_world_rows = world_rows / size;
        partial_world_start = rank * partial_world_rows;
        partial_world_end = (rank + 1) * partial_world_rows - 1;
    }
    else
    {
        int quotient = world_rows / size;
        int remainder = world_rows % size;
        if (rank < remainder)
        {
            partial_world_rows = quotient + 1;
            partial_world_start = rank * partial_world_rows;
            partial_world_end = (rank + 1) * partial_world_rows - 1;
        }
        else
        {
            partial_world_rows = quotient;
            partial_world_start = remainder * (quotient + 1) + (rank - remainder) * quotient;
            partial_world_end = remainder * (quotient + 1) + (rank - remainder + 1) * quotient - 1;
        }
    }
    partial_world_cols = world_cols;

    for (h = 0; h < HISTORY; h++)
    {
        partial_worlds[h].rows = partial_world_rows;
        partial_worlds[h].cols = partial_world_cols;
        partial_worlds[h].cells = alloc_2d_int_array(partial_world_rows + 2, partial_world_cols + 2);
    }

    cur_partial_world = &partial_worlds[world_iter % HISTORY];

    // use the received world to initialize the partial world
    for (int i = 1; i <= partial_world_rows; i++)
    {
        for (int j = 1; j <= partial_world_cols; j++)
        {
            cur_partial_world->cells[i][j] = cur_world->cells[i + partial_world_start][j];
        }
    }

    if (rank == 0)
        start_time = time_secs();

    /*  time steps */
    partial_world_border_wrap(cur_partial_world);
    for (world_iter = 1; world_iter < nsteps; world_iter++)
    {
        partial_world *next_partial_world = &partial_worlds[world_iter % HISTORY];
        partial_world_timestep(cur_partial_world, next_partial_world);
        cur_partial_world = next_partial_world;

        partial_world_border_wrap(cur_partial_world);

        int cycle = partial_world_check_cycles(cur_partial_world, world_iter);
        if (rank != 0)
            MPI_Send(&cycle, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        else
        {
            for (int i = 1; i < size; i++)
            {
                int tmp;
                MPI_Recv(&tmp, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                // if all ranks are in cycles, then the whole world is in a cycle
                cycle = cycle == tmp ? cycle : 0;
            }
            if (cycle)
                printf("world iteration %d is equal to iteration %d\n", world_iter, cycle);
        }

        if (print_cells > 0 && (world_iter % print_cells) == (print_cells - 1))
        {
            collect_world();
            if (rank == 0)
                printf("%d: %d live cells\n", world_iter, world_count(cur_world));
        }

        if (print_world > 0 && (world_iter % print_world) == (print_world - 1))
        {
            collect_world();
            if (rank == 0)
            {
                printf("\nat time step %d:\n\n", world_iter);
                world_print(cur_world);
            }
        }

        if (rank == 0)
        {
            for (int i = 1; i < size; i++)
                MPI_Send(&cycle, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            if (cycle)
            {
                if (print_world > 0)
                {
                    collect_world();
                    world_print(cur_world);
                }
                break;
            }
        }
        else
        {
            int tmp;
            MPI_Recv(&tmp, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (tmp)
            {
                if (print_world > 0)
                    collect_world();
                break;
            }
        }
    }

    if (rank == 0)
    {
        end_time = time_secs();
        elapsed_time = end_time - start_time;
    }

    collect_world();

    if (rank == 0)
    {
        /*  Iterations are done; sum the number of live cells */
        printf("Number of live cells = %d\n", world_count(cur_world));
        fprintf(stderr, "Game of Life took %10.3f seconds\n", elapsed_time);
    }

    MPI_Finalize();

    return 0;
}
