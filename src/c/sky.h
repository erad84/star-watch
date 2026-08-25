#pragma once

#include <pebble.h>
#include <stdbool.h>

#define SKY_HORIZON_STEPS 24

void sky_set_bounds(GSize size);
void sky_set_observer(float lat_deg, float lon_deg);
void sky_set_look(float az_deg, float alt_deg);
void sky_update_time(void);
void sky_refresh_stars(void);

bool sky_project_equatorial(float ra_deg, float dec_deg, int16_t *px, int16_t *py);
float sky_alt_equatorial(float ra_deg, float dec_deg);
bool sky_project_horiz(float az_deg, float alt_deg, int16_t *px, int16_t *py);
bool sky_horizon_point(int i, int16_t *px, int16_t *py);
bool sky_ecliptic_point(int i, int16_t *px, int16_t *py);
bool sky_star_on_screen(int index, int16_t *px, int16_t *py);
int16_t sky_star_alt_deg(int index);
void sky_aim_equatorial(float ra_deg, float dec_deg, int16_t *px, int16_t *py, bool *on_screen);
void sky_aim_from_horiz(float az_deg, float alt_deg, int16_t *px, int16_t *py, bool *on_screen);
int sky_ang_radius_px(float diam_deg);
