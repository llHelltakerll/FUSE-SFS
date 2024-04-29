#include "bitmap.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void create_bitmap(bitmap_t* bitmap, int size)
{
    bitmap->bits
        = (unsigned int*)calloc((size + 31) / 32, sizeof(unsigned int));
    bitmap->size = size;
}

void destroy_bitmap(bitmap_t* bitmap)
{
    free(bitmap->bits);
}

bool is_bit_set(unsigned int* bits, int pos)
{
    int index = pos / 32;
    int bitPos = pos % 32;
    unsigned int mask = 1 << bitPos;
    return (bits[index] & mask) != 0;
}

void set_bit(unsigned int* bits, int pos)
{
    int index = pos / 32;
    int bitPos = pos % 32;
    unsigned int mask = 1 << bitPos;
    bits[index] |= mask;
}

void clear_bit(unsigned int* bits, int pos)
{
    int index = pos / 32;
    int bitPos = pos % 32;
    unsigned int mask = ~(1 << bitPos);
    bits[index] &= mask;
}

int allocate_elements(bitmap_t* bitmap, int numElements)
{
    int count = 0;
    int start = -1;

    for (int i = 0; i < bitmap->size; i++) {
        if (!is_bit_set(bitmap->bits, i)) {
            if (count == 0) { start = i; }

            count++;

            if (count == numElements) {
                for (int j = start; j < start + numElements; j++) {
                    set_bit(bitmap->bits, j);
                }
                return start;
            }
        }
        else {
            count = 0;
            start = -1;
        }
    }

    return -1;
}

void remove_elements(bitmap_t* bitmap, int start, int numElements)
{
    for (int i = start; i < start + numElements; i++) {
        clear_bit(bitmap->bits, i);
    }
}

void print_bitmap(bitmap_t* bitmap)
{
    for (int i = 0; i < bitmap->size; i++) {
        printf("%d", is_bit_set(bitmap->bits, i));
    }
    printf("\n");
}
