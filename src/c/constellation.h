#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
  FAMILY_URSA_MAJOR = 0,
  FAMILY_ZODIAC,
  FAMILY_PERSEUS,
  FAMILY_HERCULES,
  FAMILY_ORION,
  FAMILY_WATERS,
  FAMILY_BAYER,
  FAMILY_LACAILLE,
  FAMILY_COUNT
};

int constellation_count(void);
const char *constellation_name(int index);
void constellation_center(int index, float *ra_deg, float *dec_deg);
int constellation_seg_count(int index);
void constellation_seg_equatorial(int index, int seg,
                                  float *ra0, float *dec0,
                                  float *ra1, float *dec1);

int constellation_family_count(void);
const char *constellation_family_name(int family);
int constellation_family_member_count(int family);
int constellation_family_member(int family, int member);
bool constellation_is_zodiac(int index);
