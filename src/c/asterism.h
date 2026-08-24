#pragma once

void asterism_init(void);
int asterism_count(void);
const char *asterism_name(int index);
void asterism_center(int index, float *ra_deg, float *dec_deg);
int asterism_seg_count(int index);
void asterism_seg_equatorial(int index, int seg,
                             float *ra0, float *dec0,
                             float *ra1, float *dec1);
