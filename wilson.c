// gcc -g -Wall --std=c99 wilson.c -lpng -o wilson && ./wilson

/* Generate a random spanning tree of the RGB cube, and a random
   spanning tree of the pixel grid, using Wilson's algorithm, then
   do a simultaneous breadth-first search of these trees to obtain
   a bijection between the RGB cube and the pixel grid.

   Inspired by allrgb.com.

   -- Robin Houston, 2014-03
 */

#include <bitstring.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#define N_BITS 24
#define N (1 << N_BITS)

#define _t3(i, j, k) ( ((i & 0xFF) << 16) | ((j & 0xFF) << 8) | (k & 0xFF) )
#define _x3(n) ((n >> 16) & 0xFF)
#define _y3(n) ((n >> 8) & 0xFF)
#define _z3(n) (n & 0xFF)

#define _t2(i, j) ( ((i & 0xFFF) << 12) | (j & 0xFFF) )
#define _x2(n) ((n >> 12) & 0xFFF)
#define _y2(n) (n & 0xFFF)

typedef struct {
    int adjacent[N][7]; // length-prefixed
    
    int queue_start;
    int queue_end;
    int queue[N];
} maze;

bool has_next(maze *m) {
    return m->queue_end > m->queue_start;
}
int next(maze *m) {
    int r = m->queue[m->queue_start++];
    int *adjacent_to_r = m->adjacent[r];
    for (int i=0; i < adjacent_to_r[0]; i++) {
        m->queue[m->queue_end++] = adjacent_to_r[i + 1];
    }
    
    return r;
}

int random_3d_neighbour(int cell) {
    int neighbours[6];
    int num_neighbours = 0;
    int x = _x3(cell),
        y = _y3(cell),
        z = _z3(cell);
    
    if (x > 0) neighbours[num_neighbours++] = _t3(x-1, y, z);
    if (x < 0xFF) neighbours[num_neighbours++] = _t3(x+1, y, z);
    
    if (y > 0) neighbours[num_neighbours++] = _t3(x, y-1, z);
    if (y < 0xFF) neighbours[num_neighbours++] = _t3(x, y+1, z);
    
    if (z > 0) neighbours[num_neighbours++] = _t3(x, y, z-1);
    if (z < 0xFF) neighbours[num_neighbours++] = _t3(x, y, z+1);
    
    while (1) {
        int r = rand() & 0x7;
        if (r < num_neighbours) return neighbours[r];
    }
}

int random_2d_neighbour(int cell) {
    int neighbours[4];
    int num_neighbours = 0;
    int x = _x2(cell),
        y = _y2(cell);
    
    if (x > 0) neighbours[num_neighbours++] = _t2(x-1, y);
    if (x < 0xFFF) neighbours[num_neighbours++] = _t2(x+1, y);
    
    if (y > 0) neighbours[num_neighbours++] = _t2(x, y-1);
    if (y < 0xFFF) neighbours[num_neighbours++] = _t2(x, y+1);
    
    while (1) {
        int r = rand() & 0x7;
        if (r < num_neighbours) return neighbours[r];
    }
}


maze *generate_maze(int start_cell, int random_neighbour(int)) {
    maze *m = calloc(sizeof(maze), 1);
    m->queue_start = 0;
    m->queue_end = 1;
    m->queue[0] = start_cell;
    
    int *pointers = calloc(sizeof(int), N);
    
    bitstr_t *seen = bit_alloc(N);
    bit_nclear(seen, 0, N-1);
    bit_set(seen, start_cell);
    
    int n = 1;
    int s = 0;
    int percent_done = -1;
    int *path = malloc(sizeof(int) * N);
    int path_len = 0;
    
    while (n < N) {
        int percent_now_done = n * 100 / N;
        if (percent_now_done > percent_done) {
            printf("%d%% complete\n", percent_now_done);
            percent_done = percent_now_done;
        }
        
        path_len = 1;
        path[0] = s++;
        while (!bit_test(seen, path[path_len - 1])) {
            pointers[path[path_len - 1]] = -path_len;
            int next_cell = random_neighbour(path[path_len - 1]);
            if (pointers[next_cell] < 0) {
                int truncation = -pointers[next_cell];
                for (int i=truncation; i < path_len; i++) {
                    pointers[path[i]] = 0;
                }
                path_len = truncation;
            }
            else {
                path[path_len++] = next_cell;
            }
        }
        
        // Incorporate path
        for (int i=0; i < path_len - 1; i++) {
            pointers[path[i]] = path[i+1];
            bit_set(seen, path[i]);
            n++;
        }
    }
    printf("100%% complete\n");
    
    for (int i=0; i<N; i++) {
        int j = pointers[i];
        if (i != start_cell) {
            int *adjacent_to_j = m->adjacent[j];
            adjacent_to_j[++adjacent_to_j[0]] = i;
        }
    }
    
    free(path);
    free(pointers);
    free(seen);
    
    return m;
}

void write_png(char *filename, int *img)
{
    FILE *fp = fopen(filename, "wb");
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    
    printf("Saving PNG to %s...\n", filename);
    
    png_init_io(png_ptr, fp);
    
    png_set_IHDR(png_ptr, info_ptr, 4096, 4096,
         8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(png_ptr, info_ptr);
    
    png_bytep row = malloc(3 * 4096 * sizeof(png_byte));
    for (int y=0; y < 4096; y++) {
        for (int x=0; x < 4096; x++) {
            int color = img[_t2(x, y)];
            row[x*3] = (color >> 16) & 0xFF;
            row[x*3 + 1] = (color >> 8) & 0xFF;
            row[x*3 + 2] = color & 0xFF;
        }
        png_write_row(png_ptr, row);
    }
    
    fclose(fp);
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    free(row);
}

int main(int argc, char **argv)
{
    sranddev();

    maze *m2 = generate_maze(_t2(2047, 2047), random_2d_neighbour);
    maze *m3 = generate_maze(_t3(127, 127, 127), random_3d_neighbour);
    
    int *img = calloc(sizeof(int), N);
    int n = 0;
    int percent_done = -1;
    
    while (has_next(m2)) {
        img[next(m2)] = next(m3);
        int percent_now_done = ++n * 100 / N;
        if (percent_now_done > percent_done) {
            printf("Image generation %d%% done\n", percent_now_done);
            percent_done = percent_now_done;
        }
    }
    
    write_png("wilson.png", img);

    return 0;
}
