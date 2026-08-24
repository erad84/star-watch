#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>

#define PLANET_COUNT 9
#define DWARF_COUNT 5
#define ASTEROID_COUNT 10
#define SAT_COUNT 1
#define FOV_DEG 50.0f
#define PERSIST_LAT 1
#define PERSIST_LON 2
#define PERSIST_DECL 3
#define PERSIST_BELOW 4
#define PERSIST_FAINT 5
#define PERSIST_BRIGHT 6
#define PERSIST_PLANETS 7
#define PERSIST_SUN 8
#define PERSIST_MOON 9
#define PERSIST_DWARFS 10
#define PERSIST_ASTEROIDS 11
#define PERSIST_SATS 12
#define PERSIST_CLUSTERS 13
#define PERSIST_GALAXIES 14
#define PERSIST_NEBULAE 15
#define PERSIST_CONSTELLS 16
#define PERSIST_ASTERISMS 17
#define PERSIST_CARDINALS 18
#define PERSIST_HEADING 19

enum {
  LIGHT_OFF = 0,
  LIGHT_WHITE,
#ifdef PBL_PLATFORM_EMERY
  LIGHT_RED,
#endif
  LIGHT_COUNT
};

enum {
  TARGET_MANUAL = 0,
  TARGET_NAMED
};

enum {
  TARGET_KIND_BODY = 0,
  TARGET_KIND_STAR,
  TARGET_KIND_CONSTELL,
  TARGET_KIND_ASTERISM,
  TARGET_KIND_DWARF,
  TARGET_KIND_ASTEROID,
  TARGET_KIND_CLUSTER,
  TARGET_KIND_GALAXY,
  TARGET_KIND_NEBULA,
  TARGET_KIND_SAT
};

enum {
  SKY_STARS = 0,
  SKY_ZODIAC,
  SKY_CONSTELL,
  SKY_MODE_COUNT
};

enum {
  BODY_SUN = 0,
  BODY_MOON,
  BODY_MERCURY,
  BODY_VENUS,
  BODY_MARS,
  BODY_JUPITER,
  BODY_SATURN,
  BODY_URANUS,
  BODY_NEPTUNE
};

typedef struct {
  float lat_deg;
  float lon_deg;
  float declination_deg;
  bool has_gps;
  bool compass_ok;
  uint8_t compass_pct;
  uint8_t gps_pct;
  bool planets_valid;
  bool dwarfs_valid;
  bool asteroids_valid;
  bool iss_valid;
  uint8_t light_mode;
  uint8_t target_mode;
  uint8_t target_kind;
  uint16_t target_index;
  bool show_below_horizon;
  bool show_faint_stars;
  bool show_bright_stars;
  bool show_planets;
  bool show_sun;
  bool show_moon;
  bool show_dwarfs;
  bool show_asteroids;
  bool show_sats;
  bool show_clusters;
  bool show_galaxies;
  bool show_nebulae;
  bool show_constellations;
  bool show_asterisms;
  bool show_cardinals;
  bool show_heading;
  uint8_t sky_mode;
  float look_az_deg;
  float look_alt_deg;
  float planet_ra_deg[PLANET_COUNT];
  float planet_dec_deg[PLANET_COUNT];
  float dwarf_ra_deg[DWARF_COUNT];
  float dwarf_dec_deg[DWARF_COUNT];
  float asteroid_ra_deg[ASTEROID_COUNT];
  float asteroid_dec_deg[ASTEROID_COUNT];
  float iss_ra_deg;
  float iss_dec_deg;
} AppState;

extern AppState g_app;
extern const char *const BODY_NAMES[PLANET_COUNT];
extern const char *const DWARF_NAMES[DWARF_COUNT];
extern const char *const ASTEROID_NAMES[ASTEROID_COUNT];
extern const char *const SAT_NAMES[SAT_COUNT];

void app_notify_target_changed(void);
void app_open_calibration(void);
