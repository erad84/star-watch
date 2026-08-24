#pragma once

int cluster_count(void);
const char *cluster_name(int index);
void cluster_equatorial(int index, float *ra_deg, float *dec_deg);
float cluster_size_deg(int index);

int galaxy_count(void);
const char *galaxy_name(int index);
void galaxy_equatorial(int index, float *ra_deg, float *dec_deg);
float galaxy_size_deg(int index);

int nebula_count(void);
const char *nebula_name(int index);
void nebula_equatorial(int index, float *ra_deg, float *dec_deg);
float nebula_size_deg(int index);
