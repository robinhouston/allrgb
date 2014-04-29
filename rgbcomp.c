// gcc -g -Wall --std=c99 rgbcomp.c -lpng -o rgbcomp

/* Combine several “All RGB” images into one.

     -- Robin Houston, 2014-03
 */

#include <bitstring.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#define N_BITS 24
#define N (1 << N_BITS)

#define _t3(i, j, k) ( ((i & 0xFF) << 16) | ((j & 0xFF) << 8) | (k & 0xFF) )
#define _r3(n) ((n >> 16) & 0xFF)
#define _g3(n) ((n >> 8) & 0xFF)
#define _b3(n) (n & 0xFF)

#define _t2(i, j) ( ((i & 0xFFF) << 12) | (j & 0xFFF) )
#define _x2(n) ((n >> 12) & 0xFFF)
#define _y2(n) (n & 0xFFF)

// A map from either RGB or XY space to RGB space
typedef enum { RGB, XY } domain_type;
typedef struct {
    domain_type domain;
    int map[N];
} map_type;

map_type *new_map() {
    map_type *map = malloc(sizeof(map_type));
    map->domain = RGB;
    for (int i=0; i<N; i++) map->map[i] = i;
    return map;
}

bool map_compose(map_type *map, char *png_filename, map_type *map_out) {
    FILE *fp = fopen(png_filename, "rb");
    if (!fp) {
        perror("Failed to open file for input");
        return false;
    }
    
    png_byte header[8];
    fread(header, sizeof(png_byte), 8, fp);
    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "Not a PNG file: %s\n", png_filename);
        return false;
    }
    
    bool ok = true;
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_infop end_info = png_create_info_struct(png_ptr);
    png_bytep row = malloc(3 * 4096 * sizeof(png_byte));
    
    png_read_info(png_ptr, info_ptr);
    
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_method, compression_method, filter_method;
    png_get_IHDR(png_ptr, info_ptr, &width, &height,
           &bit_depth, &color_type, &interlace_method,
           &compression_method, &filter_method);
    
    if (width != 4096 || height != 4096) {
        fprintf(stderr, "Image is not 4096 x 4096 pixels: %s\n", png_filename);
        ok = false;
        goto done;
    }
    
    if (bit_depth != 8) {
        fprintf(stderr, "Image is not 8-bit color: %s\n", png_filename);
        ok = false;
        goto done;
    }
    
    if (color_type != PNG_COLOR_TYPE_RGB) {
        fprintf(stderr, "Image does not have RGB color type: %s\n", png_filename);
        ok = false;
        goto done;
    }
    
    if (interlace_method != 0) {
        fprintf(stderr, "Sorry, we can’t yet handle interlaced inputs: %s\n", png_filename);
        ok = false;
        goto done;
    }
    
    if (map->domain == XY) map_out->domain = RGB;
    else if (map->domain == RGB) map_out->domain = XY;
    
    for (int y=0; y<4096; y++) {
        png_read_row(png_ptr, row, 0);
        for (int x=0; x<4096; x++) {
            int xy = _t2(x, y);
            int rgb = _t3(row[3*x], row[3*x + 1], row[3*x + 2]);
            switch (map->domain) {
            case XY:
                map_out->map[rgb] = map->map[xy];
                break;
            case RGB:
                map_out->map[xy] = map->map[rgb];
                break;
            }
        }
    }
    
    done:
    free(row);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fp);
    return ok;
}

bool map_write(map_type *map, char *output_filename) {
    if (map->domain != XY) {
        fprintf(stderr, "Only an XY->RGB map can be exported as a PNG\n");
        return false;
    }
    
    FILE *fp = fopen(output_filename, "wb");
    if (!fp) {
        perror("Failed to open file for output");
        return false;
    }
    
    bool ok = true;
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    
    png_init_io(png_ptr, fp);
    
    png_set_IHDR(png_ptr, info_ptr, 4096, 4096,
         8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(png_ptr, info_ptr);
    
    png_bytep row = malloc(3 * 4096 * sizeof(png_byte));
    for (int y=0; y < 4096; y++) {
        for (int x=0; x < 4096; x++) {
            int color = map->map[_t2(x, y)];
            row[x*3] = _r3(color);
            row[x*3 + 1] = _g3(color);
            row[x*3 + 2] = _b3(color);
        }
        png_write_row(png_ptr, row);
    }
    
    fclose(fp);
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    free(row);
    return ok;
}

int main(int argc, char **argv)
{
    char *output_filename = "rgbcomp.png";
    
    static struct option longopts[] = {
         { "output", required_argument, NULL, 'o' }, /* Output filename */
         { NULL, 0, NULL, 0 }
    };
    char ch;

    while ((ch = getopt_long(argc, argv, "o:", longopts, NULL)) != -1) {
        switch (ch) {
            case 'o':
                output_filename = optarg;
                break;
            default:
                fprintf(stderr,"Usage: %s [-o output file] input files...\n", argv[0]);
                exit(1);
        }
    }
    
    char **input_filenames = argv + optind;
    int num_input_filenames = argc - optind;
    
    if (num_input_filenames % 2 == 0) {
        fprintf(stderr,"%s: expected an odd number of input files\n", argv[0]);
        exit(1);
    }
    
    bool ok = true;
    map_type *maps[2] = { new_map(), malloc(sizeof(map_type)) };
    for (int i=0; i<num_input_filenames; i++) {
        printf("Reading %s...\n", input_filenames[i]);
        ok = map_compose(maps[i % 2], input_filenames[i], maps[(i + 1) % 2]);
        if (!ok) {
            fprintf(stderr, "%s: Failed to compose with image %s\n", argv[0], input_filenames[i]);
            exit(1);
        }
    }
    
    printf("Writing %s...\n", output_filename);
    ok = map_write(maps[1], output_filename);
    if (!ok) {
        fprintf(stderr, "%s: Failed to write map to %s\n", argv[0], output_filename);
        exit(1);
    }
    
    return 0;
}
