#pragma once

#include <pebble.h>
#include <stdint.h>

typedef struct {
  uint16_t ra;
  int16_t dec;
  int8_t mag10;
  uint8_t name_id;
} PackedStar;

int catalog_star_count(void);
const PackedStar *catalog_star(int index);
const char *catalog_name(uint8_t name_id);
float catalog_ra_deg(const PackedStar *s);
float catalog_dec_deg(const PackedStar *s);
float catalog_mag(const PackedStar *s);

void catalog_build_named_index(void);
int catalog_named_count(void);
int catalog_named_star_index(int sorted_i);
int catalog_bright_count(void);
int catalog_bright_star_index(int sorted_i);
int catalog_faint_count(void);
int catalog_faint_star_index(int sorted_i);
int catalog_star_by_name(const char *name);
