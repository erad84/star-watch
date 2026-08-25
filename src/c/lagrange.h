#pragma once

#include <pebble.h>

#define LAGRANGE_COUNT 10

int lagrange_count(void);
const char *lagrange_name(int index);
void lagrange_equatorial(int index, float *ra_deg, float *dec_deg);
float lagrange_size_deg(int index);
bool lagrange_ready(void);
