#ifndef BITMAP_H
#define BITMAP_H

#include <stdbool.h>

typedef struct {
    unsigned int* bits;
    int size;
} bitmap_t;

void create_bitmap(bitmap_t* bitmap, int size);
void destroy_bitmap(bitmap_t* bitmap);
bool is_bit_set(unsigned int* bits, int pos);
void set_bit(unsigned int* bits, int pos);
void clear_bit(unsigned int* bits, int pos);
int allocate_elements(bitmap_t* bitmap, int num_elements);
void remove_elements(bitmap_t* bitmap, int start, int num_elements);
void print_bitmap(bitmap_t* bitmap);

#endif // BITMAP_H
