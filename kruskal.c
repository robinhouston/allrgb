// gcc -g -Wall --std=c99 kruskal.c -lpng -o kruskal && ./kruskal

/* Generate a random spanning tree of the RGB cube, and a random
   spanning tree of the pixel grid, using Kruskal's algorithm, then
   do a simultaneous breadth-first search of these trees to obtain
   a bijection between the RGB cube and the pixel grid.

   Inspired by allrgb.com.

   -- Robin Houston, 2014-04
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
    
    int parents[N]; // A union-find tree
    int ranks[N];   // of the cells
    
    bitstr_t visited[bitstr_size(N)];
    int queue_start;
    int queue_end;
    int queue[N];
} maze;

int find_root(maze *m, int cell) {
    if (m->parents[cell] != cell) m->parents[cell] = find_root(m, m->parents[cell]);
    return m->parents[cell];
}

bool cells_are_connected(maze *m, int a, int b) {
    return find_root(m, a) == find_root(m, b);
}

void connect_cells(maze *m, int a, int b) {
    int ra = find_root(m, a), rb = find_root(m, b);
    if (ra == rb) return;
    
    if (m->ranks[ra] < m->ranks[rb]) m->parents[ra] = rb;
    else if (m->ranks[rb] < m->ranks[ra]) m->parents[rb] = ra;
    else {
        m->parents[rb] = ra;
        m->ranks[ra] += 1;
    }
    
    int *adjacent_to_a = m->adjacent[a];
    adjacent_to_a[++adjacent_to_a[0]] = b;

    int *adjacent_to_b = m->adjacent[b];
    adjacent_to_b[++adjacent_to_b[0]] = a;
}

bool has_next(maze *m) {
    return m->queue_end > m->queue_start;
}
int next(maze *m) {
    int r = m->queue[m->queue_start++];
    int *adjacent_to_r = m->adjacent[r];
    for (int i=0; i < adjacent_to_r[0]; i++) {
        int cell = adjacent_to_r[i + 1];
        if (!bit_test(m->visited, cell)) {
            m->queue[m->queue_end++] = cell;
        }
    }
    
    bit_set(m->visited, r);
    return r;
}

maze *generate_maze(int start_cell, int *from_edges, int *to_edges, int n_edges) {
    maze *m = calloc(sizeof(maze), 1);
    m->queue_start = 0;
    m->queue_end = 1;
    m->queue[0] = start_cell;
    
    for (int i=0; i<N; i++) {
      m->parents[i] = i;
      m->ranks[i] = 0;
    }
    
    // Fisher-Yates shuffle
    int mask = n_edges - 1;
    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;
    mask |= mask >> 16;
    for (int i=n_edges-1; i>=1; i--) {
        int j, from_temp, to_temp;
        
        do { j = rand() & mask; } while (j >= i);
        
        from_temp = from_edges[i];
        to_temp = to_edges[i];
        
        from_edges[i] = from_edges[j];
        to_edges[i] = to_edges[j];
        
        from_edges[j] = from_temp;
        to_edges[j] = to_temp;
    }
    
    for (int i=0; i<n_edges; i++) {
        connect_cells(m, from_edges[i], to_edges[i]);
    }
    
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

    maze *m2, *m3;
    int *from_edges, *to_edges;
    int i, j, k, n;

    from_edges = malloc(4095 * 4096 * 2 * sizeof(int));
    to_edges = malloc(4095 * 4096 * 2 * sizeof(int));
    n = 0;
    for (i=0; i<4095; i++) {
        for (j=0; j<4096; j++) {
            from_edges[n] = _t2(i, j);
            to_edges[n] = _t2(i+1, j);
            n++;
            
            from_edges[n] = _t2(j, i);
            to_edges[n] = _t2(j, i+1);
            n++;
        }
    }
    m2 = generate_maze(_t2(2047, 2047), from_edges, to_edges, 4095 * 4096 * 2);
    free(from_edges);
    free(to_edges);

    from_edges = malloc(255 * 256 * 256 * 3 * sizeof(int));
    to_edges = malloc(255 * 256 * 256 * 3 * sizeof(int));
    n = 0;
    for (i=0; i<255; i++) {
        for (j=0; j<256; j++) {
            for (k=0; k<256; k++) {
                from_edges[n] = _t3(i, j, k);
                to_edges[n] = _t3(i+1, j, k);
                n++;
                
                from_edges[n] = _t3(j, i, k);
                to_edges[n] = _t3(j, i+1, k);
                n++;
                
                from_edges[n] = _t3(j, k, i);
                to_edges[n] = _t3(j, k, i+1);
                n++;
            }
        }
    }
    m3 = generate_maze(_t3(127, 127, 127), from_edges, to_edges, 255 * 256 * 256 * 3);
    free(from_edges);
    free(to_edges);

    int *img = calloc(sizeof(int), N);
    int percent_done = -1;
    n = 0;

    while (has_next(m2)) {
        img[next(m2)] = next(m3);
        int percent_now_done = ++n * 100 / N;
        if (percent_now_done > percent_done) {
            printf("Image generation %d%% done\n", percent_now_done);
            percent_done = percent_now_done;
        }
    }
    
    write_png("kruskal.png", img);
    
    return 0;
}
