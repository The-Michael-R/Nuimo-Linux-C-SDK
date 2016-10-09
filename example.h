#include <stdio.h>
#include <string.h>
#include <glib-unix.h>

#include "nuimo.h"


void bmp_to_array(const unsigned char *bmp, unsigned char *array);
void my_cb_function(unsigned int chr, int value, unsigned int dir, void *user_data);
int  main (int argc, char **argv);
