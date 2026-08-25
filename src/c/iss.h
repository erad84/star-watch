#pragma once

#include <stdbool.h>
#include <stdint.h>

void iss_set_tle(const uint8_t *data, int length);
bool iss_ready(void);
bool iss_ready_index(int index);
bool iss_horiz(float *az_deg, float *alt_deg);
void iss_update(void);
