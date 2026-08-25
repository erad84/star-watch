#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>

#define PLANET_COUNT 9
#define DWARF_COUNT 5
#define ASTEROID_COUNT 10
#define SAT_NAMED_COUNT 18
#define SAT_GPS_COUNT 32
#define SAT_COUNT (SAT_NAMED_COUNT + SAT_GPS_COUNT)
#define SAT_GPS_0 SAT_NAMED_COUNT

enum {
  SAT_ISS = 0,
  SAT_TIANGONG,
  SAT_SDO,
  SAT_HUBBLE,
  SAT_CHANDRA,
  SAT_XMM,
  SAT_GAIA,
  SAT_TESS,
  SAT_FERMI,
  SAT_SWIFT,
  SAT_JWST,
  SAT_EUCLID,
  SAT_DSCOVR,
  SAT_NEW_HORIZONS,
  SAT_VOYAGER1,
  SAT_VOYAGER2,
  SAT_PIONEER10,
  SAT_PIONEER11
};

enum {
  MADE_STATIONS = 0,
  MADE_TELESCOPES,
  MADE_INTERSTELLAR,
  MADE_GPS,
  MADE_COUNT
};
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
#define PERSIST_TOUCH 20
#define PERSIST_ECLIPTIC 21
#define PERSIST_GPS 22
#define PERSIST_LAGRANGE 23

#if defined(PBL_COMPASS)
#define APP_HAS_LOOK_SENSORS 1
#else
#define APP_HAS_LOOK_SENSORS 0
#endif

#if defined(PBL_TOUCH) && APP_HAS_LOOK_SENSORS
#define APP_TOUCH_SETTING 1
#else
#define APP_TOUCH_SETTING 0
#endif

#if defined(PBL_TOUCH) && !APP_HAS_LOOK_SENSORS
#define APP_TOUCH_ONLY 1
#else
#define APP_TOUCH_ONLY 0
#endif

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
  TARGET_KIND_SAT,
  TARGET_KIND_LAGRANGE
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
  bool sats_valid;
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
  bool show_gps;
  bool show_lagrange;
  bool show_clusters;
  bool show_galaxies;
  bool show_nebulae;
  bool show_constellations;
  bool show_asterisms;
  bool show_cardinals;
  bool show_heading;
  bool show_ecliptic;
  bool touch_look;
  uint8_t sky_mode;
  float look_az_deg;
  float look_alt_deg;
  float planet_ra_deg[PLANET_COUNT];
  float planet_dec_deg[PLANET_COUNT];
  float dwarf_ra_deg[DWARF_COUNT];
  float dwarf_dec_deg[DWARF_COUNT];
  float asteroid_ra_deg[ASTEROID_COUNT];
  float asteroid_dec_deg[ASTEROID_COUNT];
  float sat_ra_deg[SAT_COUNT];
  float sat_dec_deg[SAT_COUNT];
  uint8_t sat_ok[SAT_COUNT];
} AppState;

extern AppState g_app;
extern const char *const BODY_NAMES[PLANET_COUNT];
extern const char *const DWARF_NAMES[DWARF_COUNT];
extern const char *const ASTEROID_NAMES[ASTEROID_COUNT];
extern const char *const SAT_NAMES[SAT_NAMED_COUNT];

const char *sat_name(int index);
bool sat_has_pos(int index);
int sat_object_count(void);
int sat_category_count(void);
const char *sat_category_name(int cat);
int sat_category_member_count(int cat);
int sat_category_member(int cat, int row);

void app_notify_target_changed(void);
void app_open_calibration(void);
void app_set_touch_look(bool on);
