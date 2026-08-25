#include "starwatch.h"
#include "draw.h"
#include "sky.h"
#include "catalog.h"
#include "asterism.h"
#include "iss.h"
#include "menu.h"
#if APP_HAS_LOOK_SENSORS
#include "compass_calibration_window.h"
#endif

#include <string.h>

AppState g_app;

const char *const BODY_NAMES[PLANET_COUNT] = {
  "Sun", "Moon", "Mercury", "Venus", "Mars",
  "Jupiter", "Saturn", "Uranus", "Neptune"
};

const char *const DWARF_NAMES[DWARF_COUNT] = {
  "Ceres", "Pluto", "Haumea", "Makemake", "Eris"
};

const char *const ASTEROID_NAMES[ASTEROID_COUNT] = {
  "Vesta", "Pallas", "Juno", "Hygiea", "Eros",
  "Psyche", "Bennu", "Apophis", "Itokawa", "Ryugu"
};

const char *const SAT_NAMES[SAT_NAMED_COUNT] = {
  "ISS", "Tiangong",
  "SDO", "Hubble", "Chandra", "XMM-Newton", "Gaia", "TESS",
  "Fermi", "Swift", "JWST", "Euclid", "DSCOVR",
  "New Horizons",
  "Voyager 1", "Voyager 2", "Pioneer 10", "Pioneer 11"
};

static const char GPS_NAMES[SAT_GPS_COUNT][7] = {
  "GPS 01", "GPS 02", "GPS 03", "GPS 04", "GPS 05", "GPS 06", "GPS 07", "GPS 08",
  "GPS 09", "GPS 10", "GPS 11", "GPS 12", "GPS 13", "GPS 14", "GPS 15", "GPS 16",
  "GPS 17", "GPS 18", "GPS 19", "GPS 20", "GPS 21", "GPS 22", "GPS 23", "GPS 24",
  "GPS 25", "GPS 26", "GPS 27", "GPS 28", "GPS 29", "GPS 30", "GPS 31", "GPS 32"
};

static const uint8_t SAT_CAT_LO[] = {
  SAT_ISS, SAT_SDO, SAT_NEW_HORIZONS, SAT_GPS_0
};
static const uint8_t SAT_CAT_HI[] = {
  SAT_SDO, SAT_NEW_HORIZONS, SAT_GPS_0, SAT_COUNT
};
static const char *const SAT_CAT_NAMES[MADE_COUNT] = {
  "Space Stations", "Telescopes", "Interstellar-bound", "GPS"
};
static const uint8_t TELESCOPE_ORDER[] = {
  SAT_HUBBLE, SAT_JWST, SAT_TESS, SAT_CHANDRA, SAT_GAIA,
  SAT_FERMI, SAT_XMM, SAT_SWIFT, SAT_SDO, SAT_EUCLID, SAT_DSCOVR
};
#define TELESCOPE_N ((int)(sizeof(TELESCOPE_ORDER) / sizeof(TELESCOPE_ORDER[0])))

const char *sat_name(int index) {
  if (index >= 0 && index < SAT_NAMED_COUNT) {
    return SAT_NAMES[index];
  }
  if (index >= SAT_GPS_0 && index < SAT_COUNT) {
    return GPS_NAMES[index - SAT_GPS_0];
  }
  return "Sat";
}

bool sat_has_pos(int index) {
  if (index < 0 || index >= SAT_COUNT) {
    return false;
  }
  return g_app.sat_ok[index] != 0;
}

int sat_object_count(void) {
  return SAT_COUNT;
}

int sat_category_count(void) {
  return MADE_COUNT;
}

const char *sat_category_name(int cat) {
  if (cat < 0 || cat >= MADE_COUNT) {
    return "Man made";
  }
  return SAT_CAT_NAMES[cat];
}

int sat_category_member_count(int cat) {
  if (cat < 0 || cat >= MADE_COUNT) {
    return 0;
  }
  if (cat == MADE_TELESCOPES) {
    return TELESCOPE_N;
  }
  if (cat == MADE_GPS) {
    return SAT_GPS_COUNT;
  }
  return (int)SAT_CAT_HI[cat] - (int)SAT_CAT_LO[cat];
}

int sat_category_member(int cat, int row) {
  int lo;
  int hi;
  if (cat < 0 || cat >= MADE_COUNT || row < 0) {
    return -1;
  }
  if (cat == MADE_TELESCOPES) {
    if (row >= TELESCOPE_N) {
      return -1;
    }
    return TELESCOPE_ORDER[row];
  }
  lo = SAT_CAT_LO[cat];
  hi = SAT_CAT_HI[cat];
  if (lo + row >= hi) {
    return -1;
  }
  return lo + row;
}

static Window *s_window;
static Layer *s_sky_layer;
static AppTimer *s_draw_timer;
#if APP_HAS_LOOK_SENSORS
static AppTimer *s_cal_done_timer;
static CompassCalibrationWindow *s_cal_window;
static bool s_cal_dismissed;
static bool s_ring_complete;
static CompassStatus s_compass_status;
#endif
static bool s_cal_visible;
static bool s_did_refresh;
static int32_t s_gps_acc_m = -1;

static uint8_t gps_percent(bool has, int32_t meters);

static bool s_az_filt_ready;
static bool s_alt_filt_ready;
static bool s_compass_ready;
static float s_az_fused;
static float s_az_rate_dps;
static float s_compass_az;
static float s_alt_lp;
static float s_alt_target;
static float s_alt_unf;
static bool s_over_pole;
static uint32_t s_compass_ms;
static bool s_sensors_on;
#ifdef PBL_TOUCH
static bool s_touch_on;
static bool s_touch_down;
static int16_t s_touch_x;
static int16_t s_touch_y;
#endif
static bool s_look_was_touch;

#define LOOK_TICK_MS 20
#define ALT_LP_ALPHA 0.28f
#define ALT_CHASE_ALPHA 0.24f
#define LOOK_EPS 0.03f
#define RATE_DECAY 0.90f
#define COMPASS_CORR_SLOW 0.03f
#define COMPASS_CORR_TURN 0.22f

static void request_draw(void);

#if APP_HAS_LOOK_SENSORS
static void accel_handler(AccelRawData *data, uint32_t num_samples, uint64_t timestamp);
static void compass_handler(CompassHeadingData heading);
#endif
static void apply_look_input_mode(void);

static uint32_t now_ms(void) {
  time_t sec;
  uint16_t ms;
  time_ms(&sec, &ms);
  return ((uint32_t)sec * 1000u) + (uint32_t)ms;
}

static void request_draw(void) {
  if (s_sky_layer) {
    layer_mark_dirty(s_sky_layer);
  }
}

static float wrap360f(float deg) {
  while (deg < 0.0f) {
    deg += 360.0f;
  }
  while (deg >= 360.0f) {
    deg -= 360.0f;
  }
  return deg;
}

static float wrap180f(float deg) {
  while (deg > 180.0f) {
    deg -= 360.0f;
  }
  while (deg < -180.0f) {
    deg += 360.0f;
  }
  return deg;
}

/* Map pitch past zenith/nadir onto alt [-90, 90] and flip heading. */
static void fold_look(float *az, float *alt) {
  float a = *alt;
  bool over = s_over_pole;
  if (!over) {
    if (a > 91.0f || a < -91.0f) {
      over = true;
    }
  } else if (a < 89.0f && a > -89.0f) {
    over = false;
  }
  s_over_pole = over;
  if (over) {
    if (a >= 0.0f) {
      a = 180.0f - a;
    } else {
      a = -180.0f - a;
    }
    *az = wrap360f(*az + 180.0f);
  }
  if (a > 90.0f) {
    a = 90.0f;
  } else if (a < -90.0f) {
    a = -90.0f;
  }
  *alt = a;
}

static void publish_look(void) {
  float az = s_az_fused;
  float alt = s_alt_unf;
  fold_look(&az, &alt);
  g_app.look_az_deg = az;
  g_app.look_alt_deg = alt;
}

static float trig_to_deg(int32_t trig) {
  return wrap360f(((float)trig * 360.0f) / (float)TRIG_MAX_ANGLE);
}

static float circ_delta(float from, float to) {
  float d = to - from;
  if (d > 180.0f) {
    d -= 360.0f;
  } else if (d < -180.0f) {
    d += 360.0f;
  }
  return d;
}

static float absf(float v) {
  return v < 0.0f ? -v : v;
}

static float chase_alpha(float abs_d, float slow, float mid, float fast) {
  if (abs_d > 18.0f) {
    return fast;
  }
  if (abs_d > 7.0f) {
    return mid;
  }
  return slow;
}

static void blend_az_rate(float inst_dps) {
  if (inst_dps > 420.0f) {
    inst_dps = 420.0f;
  } else if (inst_dps < -420.0f) {
    inst_dps = -420.0f;
  }
  s_az_rate_dps = (0.40f * s_az_rate_dps) + (0.60f * inst_dps);
}

static bool chase_look(void) {
  bool moved = false;
  float d;
  float a;
  float dt = (float)LOOK_TICK_MS / 1000.0f;
  float corr;
  float az_before = s_az_fused;
  if (s_az_filt_ready) {
    s_az_fused = wrap360f(s_az_fused + s_az_rate_dps * dt);
    if (s_compass_ready) {
      corr = circ_delta(s_az_fused, s_compass_az);
      if (absf(corr) > 18.0f) {
        s_az_fused = wrap360f(s_az_fused + COMPASS_CORR_TURN * corr);
      } else {
        s_az_fused = wrap360f(s_az_fused + COMPASS_CORR_SLOW * corr);
      }
    }
    s_az_rate_dps *= RATE_DECAY;
    if (absf(circ_delta(az_before, s_az_fused)) > LOOK_EPS) {
      moved = true;
    }
  }
  if (s_alt_filt_ready) {
    d = s_alt_target - s_alt_unf;
    a = chase_alpha(absf(d), ALT_CHASE_ALPHA, 0.36f, 0.50f);
    if (d > LOOK_EPS || d < -LOOK_EPS) {
      s_alt_unf += a * d;
      moved = true;
    }
  }
  if (moved) {
    publish_look();
  }
  return moved;
}

static bool look_still_chasing(void);

static void draw_timer_cb(void *context);

static void kick_look_timer(void) {
  if (!s_draw_timer) {
    return;
  }
  app_timer_cancel(s_draw_timer);
  s_draw_timer = app_timer_register(LOOK_TICK_MS, draw_timer_cb, NULL);
}

static bool look_still_chasing(void) {
  if (g_app.touch_look) {
    return false;
  }
  if (s_az_filt_ready && absf(s_az_rate_dps) > 0.8f) {
    return true;
  }
  if (s_az_filt_ready && s_compass_ready &&
      absf(circ_delta(s_az_fused, s_compass_az)) > LOOK_EPS) {
    return true;
  }
  if (s_alt_filt_ready && absf(s_alt_target - s_alt_unf) > LOOK_EPS) {
    return true;
  }
  return false;
}

static bool apply_look_az(float compass_az) {
  uint32_t t = now_ms();
  float d;
  float dt;
  if (g_app.touch_look) {
    return false;
  }
  compass_az = wrap360f(compass_az);
  if (!s_az_filt_ready) {
    s_az_fused = compass_az;
    s_compass_az = compass_az;
    s_az_filt_ready = true;
    s_compass_ready = true;
    s_compass_ms = t;
    publish_look();
    return true;
  }
  d = circ_delta(s_compass_az, compass_az);
  if (s_compass_ms != 0) {
    dt = ((float)(t - s_compass_ms)) / 1000.0f;
    if (dt > 0.012f && dt < 0.80f && absf(d) > 0.05f) {
      blend_az_rate(d / dt);
    }
  }
  s_compass_az = compass_az;
  s_compass_ready = true;
  s_compass_ms = t;
  {
    bool moved = chase_look();
    kick_look_timer();
    return moved;
  }
}

static bool apply_look_alt(float alt_raw) {
  float d;
  if (g_app.touch_look) {
    return false;
  }
  alt_raw = wrap180f(alt_raw);
  if (!s_alt_filt_ready) {
    s_alt_lp = alt_raw;
    s_alt_target = alt_raw;
    s_alt_unf = alt_raw;
    s_alt_filt_ready = true;
    publish_look();
    return true;
  }
  d = alt_raw - s_alt_lp;
  s_alt_lp += ALT_LP_ALPHA * d;
  s_alt_target = s_alt_lp;
  {
    bool moved = chase_look();
    kick_look_timer();
    return moved;
  }
}

static int32_t tuple_int32(Tuple *t, int32_t fallback) {
  if (!t) {
    return fallback;
  }
  if (t->length == 1) {
    return (int32_t)t->value->uint8;
  }
  if (t->length == 2) {
    return (int32_t)t->value->int16;
  }
  return t->value->int32;
}

static void persist_location(void) {
  int32_t lat_mdeg = (int32_t)(g_app.lat_deg * 1000.0f);
  int32_t lon_mdeg = (int32_t)(g_app.lon_deg * 1000.0f);
  int32_t decl_tenth = (int32_t)(g_app.declination_deg * 10.0f);
  persist_write_int(PERSIST_LAT, lat_mdeg);
  persist_write_int(PERSIST_LON, lon_mdeg);
  persist_write_int(PERSIST_DECL, decl_tenth);
}

static bool persist_flag(uint32_t key, bool fallback) {
  if (!persist_exists(key)) {
    return fallback;
  }
  return persist_read_int(key) != 0;
}

static void load_persisted_location(void) {
  g_app.show_below_horizon = persist_flag(PERSIST_BELOW, true);
  g_app.show_faint_stars = persist_flag(PERSIST_FAINT, true);
  g_app.show_bright_stars = persist_flag(PERSIST_BRIGHT, true);
  g_app.show_planets = persist_flag(PERSIST_PLANETS, true);
  g_app.show_sun = persist_flag(PERSIST_SUN, true);
  g_app.show_moon = persist_flag(PERSIST_MOON, true);
  g_app.show_dwarfs = persist_flag(PERSIST_DWARFS, true);
  g_app.show_asteroids = persist_flag(PERSIST_ASTEROIDS, true);
  g_app.show_sats = persist_flag(PERSIST_SATS, false);
  g_app.show_gps = persist_flag(PERSIST_GPS, false);
  g_app.show_lagrange = persist_flag(PERSIST_LAGRANGE, false);
  g_app.show_clusters = persist_flag(PERSIST_CLUSTERS, true);
  g_app.show_galaxies = persist_flag(PERSIST_GALAXIES, true);
  g_app.show_nebulae = persist_flag(PERSIST_NEBULAE, true);
  g_app.show_constellations = persist_flag(PERSIST_CONSTELLS, true);
  g_app.show_asterisms = persist_flag(PERSIST_ASTERISMS, true);
  g_app.show_cardinals = persist_flag(PERSIST_CARDINALS, true);
  g_app.show_heading = persist_flag(PERSIST_HEADING, true);
  g_app.show_ecliptic = persist_flag(PERSIST_ECLIPTIC, true);
#if APP_TOUCH_ONLY
  g_app.touch_look = true;
#elif APP_TOUCH_SETTING
  g_app.touch_look = persist_flag(PERSIST_TOUCH, false);
#endif
  if (!persist_exists(PERSIST_LAT) || !persist_exists(PERSIST_LON)) {
    return;
  }
  g_app.lat_deg = persist_read_int(PERSIST_LAT) / 1000.0f;
  g_app.lon_deg = persist_read_int(PERSIST_LON) / 1000.0f;
  if (persist_exists(PERSIST_DECL)) {
    g_app.declination_deg = persist_read_int(PERSIST_DECL) / 10.0f;
  }
  sky_set_observer(g_app.lat_deg, g_app.lon_deg);
}

static void request_phone_update(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_Request, 1);
  app_message_outbox_send();
}

static void apply_packed_eq(Tuple *t, float *ra, float *dec, int count, bool *valid) {
  if (!t || t->type != TUPLE_BYTE_ARRAY || t->length < count * 4) {
    return;
  }
  const uint8_t *p = t->value->data;
  for (int i = 0; i < count; i++) {
    uint16_t ra_c = (uint16_t)(p[0] | (p[1] << 8));
    int16_t dec_c = (int16_t)(p[2] | (p[3] << 8));
    ra[i] = ra_c / 100.0f;
    dec[i] = dec_c / 100.0f;
    p += 4;
  }
  *valid = true;
}

static void apply_planets(Tuple *t) {
  apply_packed_eq(t, g_app.planet_ra_deg, g_app.planet_dec_deg, PLANET_COUNT,
                  &g_app.planets_valid);
}

void app_notify_target_changed(void) {
  DictionaryIterator *iter;
  bool sat = g_app.target_mode == TARGET_NAMED &&
             g_app.target_kind == TARGET_KIND_SAT;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_TrackISS, sat ? 1 : 0);
  dict_write_int32(iter, MESSAGE_KEY_TrackSat, sat ? (int32_t)g_app.target_index : -1);
  dict_write_int32(iter, MESSAGE_KEY_Request, 1);
  app_message_outbox_send();
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  (void)context;
  Tuple *lat_t = dict_find(iter, MESSAGE_KEY_Lat);
  Tuple *lon_t = dict_find(iter, MESSAGE_KEY_Lon);
  Tuple *decl_t = dict_find(iter, MESSAGE_KEY_Declination);
  Tuple *gps_t = dict_find(iter, MESSAGE_KEY_HasGps);
  Tuple *planets_t = dict_find(iter, MESSAGE_KEY_Planets);
  Tuple *dwarfs_t = dict_find(iter, MESSAGE_KEY_Dwarfs);
  Tuple *asteroids_t = dict_find(iter, MESSAGE_KEY_Asteroids);
  Tuple *sats_t = dict_find(iter, MESSAGE_KEY_Sats);
  Tuple *iss_t = dict_find(iter, MESSAGE_KEY_ISS);
  Tuple *tle_t = dict_find(iter, MESSAGE_KEY_ISS_TLE);
  Tuple *acc_t = dict_find(iter, MESSAGE_KEY_GpsAcc);

  if (lat_t && lon_t) {
    g_app.lat_deg = tuple_int32(lat_t, 0) / 1000.0f;
    g_app.lon_deg = tuple_int32(lon_t, 0) / 1000.0f;
    sky_set_observer(g_app.lat_deg, g_app.lon_deg);
    sky_update_time();
    sky_refresh_stars();
    persist_location();
  }
  if (decl_t) {
    g_app.declination_deg = tuple_int32(decl_t, 0) / 10.0f;
    persist_write_int(PERSIST_DECL, tuple_int32(decl_t, 0));
  }
  if (gps_t || acc_t) {
    uint8_t pct;
    if (gps_t) {
      g_app.has_gps = tuple_int32(gps_t, 0) != 0;
      if (!g_app.has_gps) {
        s_gps_acc_m = -1;
      }
    }
    if (acc_t) {
      s_gps_acc_m = tuple_int32(acc_t, 0);
    }
    pct = gps_percent(g_app.has_gps, s_gps_acc_m);
    if (pct != g_app.gps_pct) {
      g_app.gps_pct = pct;
      menu_refresh();
    }
  }
  apply_planets(planets_t);
  apply_packed_eq(dwarfs_t, g_app.dwarf_ra_deg, g_app.dwarf_dec_deg, DWARF_COUNT,
                  &g_app.dwarfs_valid);
  apply_packed_eq(asteroids_t, g_app.asteroid_ra_deg, g_app.asteroid_dec_deg,
                  ASTEROID_COUNT, &g_app.asteroids_valid);
  if (sats_t && sats_t->type == TUPLE_BYTE_ARRAY &&
      sats_t->length >= SAT_COUNT * 4) {
    const uint8_t *p = sats_t->value->data;
    int i;
    for (i = 0; i < SAT_COUNT; i++) {
      uint16_t ra_c = (uint16_t)(p[0] | (p[1] << 8));
      int16_t dec_c = (int16_t)(p[2] | (p[3] << 8));
      if (dec_c == 32767) {
        g_app.sat_ok[i] = 0;
      } else {
        g_app.sat_ra_deg[i] = ra_c / 100.0f;
        g_app.sat_dec_deg[i] = dec_c / 100.0f;
        g_app.sat_ok[i] = 1;
      }
      p += 4;
    }
    g_app.sats_valid = true;
    menu_refresh();
  } else if (iss_t) {
    apply_packed_eq(iss_t, g_app.sat_ra_deg, g_app.sat_dec_deg, 1,
                    &g_app.sats_valid);
    if (g_app.sats_valid) {
      g_app.sat_ok[SAT_ISS] = 1;
    }
  }
  if (tle_t && tle_t->type == TUPLE_BYTE_ARRAY) {
    iss_set_tle(tle_t->value->data, tle_t->length);
  }
  request_draw();
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  (void)context;
  (void)reason;
}

static float altitude_from_accel(int32_t y, int32_t z) {
  float deg = trig_to_deg(atan2_lookup(z, -y));
  return wrap180f(deg);
}

static uint8_t gps_percent(bool has, int32_t meters) {
  if (!has) {
    return 0;
  }
  if (meters < 0) {
    return 70;
  }
  if (meters <= 8) {
    return 100;
  }
  if (meters >= 120) {
    return 10;
  }
  return (uint8_t)(100 - ((meters - 8) * 90) / 112);
}

#if APP_HAS_LOOK_SENSORS
static void refresh_compass_pct(void) {
  uint8_t pct = 0;
  if (s_compass_status == CompassStatusCalibrated) {
    pct = 100;
  } else if (s_compass_status == CompassStatusCalibrating) {
    pct = s_cal_window ?
          (uint8_t)compass_calibration_window_fill_percent(s_cal_window) : 40;
    if (pct < 5) {
      pct = 5;
    }
  }
  if (pct != g_app.compass_pct) {
    g_app.compass_pct = pct;
    menu_refresh();
  }
}

static void hide_calibration(bool vibe);

static void calibration_back(CompassCalibrationWindow *window) {
  (void)window;
  if (s_cal_done_timer) {
    app_timer_cancel(s_cal_done_timer);
    s_cal_done_timer = NULL;
  }
  if (s_cal_visible && s_cal_window) {
    window_stack_remove(compass_calibration_window_get_window(s_cal_window), true);
  }
  s_cal_visible = false;
  s_cal_dismissed = true;
  apply_look_input_mode();
}

static void finish_calibration(void) {
  s_ring_complete = true;
  hide_calibration(true);
}

static void cal_ring_done_cb(void *context) {
  (void)context;
  s_cal_done_timer = NULL;
  finish_calibration();
}

static void calibration_filled(CompassCalibrationWindow *window) {
  (void)window;
  if (s_cal_done_timer) {
    return;
  }
  s_cal_done_timer = app_timer_register(400, cal_ring_done_cb, NULL);
}
#endif

static void sensors_enable(void) {
#if APP_HAS_LOOK_SENSORS
  if (s_sensors_on) {
    return;
  }
  accel_raw_data_service_subscribe(1, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
  compass_service_subscribe(compass_handler);
  compass_service_set_heading_filter(0);
  s_sensors_on = true;
#endif
}

static void sensors_disable(void) {
#if APP_HAS_LOOK_SENSORS
  if (!s_sensors_on) {
    return;
  }
  compass_service_unsubscribe();
  accel_data_service_unsubscribe();
  s_sensors_on = false;
#endif
}

#ifdef PBL_TOUCH
static void pan_look(int dx, int dy) {
  float w = (float)PBL_DISPLAY_WIDTH;
  float h = (float)PBL_DISPLAY_HEIGHT;
  if (w < 1.0f) {
    w = 1.0f;
  }
  if (h < 1.0f) {
    h = 1.0f;
  }
  s_az_fused = wrap360f(s_az_fused - ((float)dx * FOV_DEG) / w);
  s_az_filt_ready = true;
  s_alt_unf = wrap180f(s_alt_unf + ((float)dy * FOV_DEG) / h);
  s_alt_lp = s_alt_unf;
  s_alt_target = s_alt_unf;
  s_alt_filt_ready = true;
  publish_look();
  request_draw();
}

static void touch_handler(const TouchEvent *event, void *context) {
  (void)context;
  if (!g_app.touch_look || !event) {
    return;
  }
  if (s_window && window_stack_get_top_window() != s_window) {
    s_touch_down = false;
    return;
  }
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_touch_down = true;
      s_touch_x = event->x;
      s_touch_y = event->y;
      break;
    case TouchEvent_PositionUpdate:
      if (s_touch_down) {
        pan_look(event->x - s_touch_x, event->y - s_touch_y);
        s_touch_x = event->x;
        s_touch_y = event->y;
      }
      break;
    case TouchEvent_Liftoff:
      s_touch_down = false;
      break;
  }
}

static void touch_listen(bool on) {
  if (on) {
    if (!s_touch_on) {
      touch_service_subscribe(touch_handler, NULL);
      s_touch_on = true;
    }
  } else if (s_touch_on) {
    touch_service_unsubscribe();
    s_touch_on = false;
    s_touch_down = false;
  }
}
#endif

static void apply_look_input_mode(void) {
  if (g_app.touch_look) {
    if (!s_cal_visible) {
      sensors_disable();
    }
    g_app.compass_ok = true;
    s_az_rate_dps = 0;
#ifdef PBL_TOUCH
    if (s_window && window_stack_get_top_window() == s_window) {
      touch_listen(true);
    }
#endif
  } else {
#ifdef PBL_TOUCH
    touch_listen(false);
#endif
    if (s_look_was_touch) {
      s_compass_az = s_az_fused;
      s_az_filt_ready = true;
      s_compass_ready = false;
      s_az_rate_dps = 0;
      s_alt_lp = s_alt_unf;
      s_alt_target = s_alt_unf;
      s_alt_filt_ready = true;
    }
    sensors_enable();
  }
  s_look_was_touch = g_app.touch_look;
}

void app_set_touch_look(bool on) {
#if APP_TOUCH_ONLY
  (void)on;
  g_app.touch_look = true;
#else
  g_app.touch_look = on;
#endif
}

#if APP_HAS_LOOK_SENSORS
static void present_calibration(bool force) {
  BatteryChargeState bat;
  if (s_cal_visible) {
    if (force && s_cal_window) {
      compass_calibration_window_reset(s_cal_window);
    }
    return;
  }
  if (!force && g_app.touch_look) {
    return;
  }
  if (!force && (s_cal_dismissed || s_ring_complete)) {
    return;
  }
  sensors_enable();
  if (!s_cal_window) {
    s_cal_window = compass_calibration_window_create();
  } else if (force) {
    compass_calibration_window_reset(s_cal_window);
  }
  compass_calibration_window_set_back_button_handler(s_cal_window, calibration_back);
  compass_calibration_window_set_filled_handler(s_cal_window, calibration_filled);
  bat = battery_state_service_peek();
  compass_calibration_window_set_influenced_by_magnetic_interference(s_cal_window, bat.is_plugged);
  window_stack_push(compass_calibration_window_get_window(s_cal_window), true);
  s_cal_visible = true;
}

void app_open_calibration(void) {
  s_cal_dismissed = false;
  s_ring_complete = false;
  present_calibration(true);
}

static void show_calibration(void) {
  present_calibration(false);
}

static void hide_calibration(bool vibe) {
  if (s_cal_done_timer) {
    app_timer_cancel(s_cal_done_timer);
    s_cal_done_timer = NULL;
  }
  if (!s_cal_visible || !s_cal_window) {
    return;
  }
  window_stack_remove(compass_calibration_window_get_window(s_cal_window), true);
  s_cal_visible = false;
  apply_look_input_mode();
  if (vibe) {
    vibes_long_pulse();
  }
}

static void accel_handler(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
  AccelData sample;
  (void)timestamp;
  if (num_samples == 0) {
    return;
  }
  if (apply_look_alt(altitude_from_accel(data[0].y, data[0].z))) {
    request_draw();
  }
  if (s_cal_visible && s_cal_window) {
    memset(&sample, 0, sizeof(sample));
    sample.x = data[0].x;
    sample.y = data[0].y;
    sample.z = data[0].z;
    compass_calibration_window_apply_accel_data(s_cal_window, sample);
    refresh_compass_pct();
  }
}

static void compass_handler(CompassHeadingData heading) {
  const bool usable = (heading.compass_status == CompassStatusCalibrating ||
                       heading.compass_status == CompassStatusCalibrated);
  s_compass_status = heading.compass_status;
  if (g_app.touch_look) {
    if (s_cal_visible) {
      if (heading.compass_status == CompassStatusCalibrated) {
        s_ring_complete = true;
        finish_calibration();
      }
      refresh_compass_pct();
    }
    return;
  }
  if (g_app.compass_ok != usable) {
    g_app.compass_ok = usable;
    request_draw();
  }
  if (heading.compass_status == CompassStatusCalibrated) {
    s_ring_complete = true;
    finish_calibration();
  } else if (!usable) {
    s_ring_complete = false;
    show_calibration();
  } else if (!s_ring_complete && !s_cal_dismissed) {
    show_calibration();
  }
  if (!usable) {
    refresh_compass_pct();
    return;
  }
  const int32_t cw_trig = TRIG_MAX_ANGLE - heading.magnetic_heading;
  float az = wrap360f(trig_to_deg(cw_trig) + g_app.declination_deg);
  if (apply_look_az(az)) {
    request_draw();
  }
  refresh_compass_pct();
}
#else
void app_open_calibration(void) {
}
#endif

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  (void)tick_time;
  (void)units_changed;
  sky_update_time();
  sky_refresh_stars();
  request_draw();
}

static void apply_light(void) {
#ifdef PBL_PLATFORM_EMERY
  switch (g_app.light_mode) {
    case LIGHT_WHITE:
      light_set_color(GColorWhite);
      light_enable(true);
      break;
    case LIGHT_RED:
      light_set_color_rgb888(0x00FF4040);
      light_enable(true);
      break;
    default:
      light_enable(false);
      light_set_system_color();
      break;
  }
#else
  light_enable(g_app.light_mode != LIGHT_OFF);
#endif
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  g_app.light_mode = (uint8_t)((g_app.light_mode + 1) % LIGHT_COUNT);
  apply_light();
  request_draw();
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  menu_open();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (g_app.target_mode == TARGET_NAMED) {
    g_app.target_mode = TARGET_MANUAL;
    g_app.sky_mode = SKY_STARS;
    app_notify_target_changed();
    request_draw();
    return;
  }
  g_app.sky_mode = (uint8_t)((g_app.sky_mode + 1) % SKY_MODE_COUNT);
  request_draw();
}

static void click_config(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void window_appear(Window *window) {
  (void)window;
  apply_look_input_mode();
}

static void window_disappear(Window *window) {
  (void)window;
#ifdef PBL_TOUCH
  touch_listen(false);
#endif
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  sky_set_bounds(bounds.size);

  s_sky_layer = layer_create(bounds);
  layer_set_update_proc(s_sky_layer, draw_sky_update);
  layer_add_child(root, s_sky_layer);
#ifdef PBL_TOUCH
  window_set_touch_bridge_disabled(window, true);
#endif
}

static bool iss_needs_anim(void) {
  if (!iss_ready()) {
    return false;
  }
  if ((g_app.show_sats || g_app.show_gps) && g_app.sky_mode == SKY_STARS) {
    return true;
  }
  if (g_app.target_mode == TARGET_NAMED && g_app.target_kind == TARGET_KIND_SAT) {
    return true;
  }
  return false;
}

static void draw_timer_cb(void *context) {
  bool chasing;
  bool iss;
  (void)context;
  chasing = g_app.touch_look ? false : chase_look();
  iss = iss_needs_anim();
  s_draw_timer = app_timer_register(
      (chasing || look_still_chasing() || iss) ? LOOK_TICK_MS : 400,
      draw_timer_cb, NULL);
  if (!s_did_refresh) {
    s_did_refresh = true;
    sky_update_time();
    sky_refresh_stars();
    request_draw();
    return;
  }
  if (chasing || iss) {
    request_draw();
  }
}

static void window_unload(Window *window) {
  (void)window;
  if (s_draw_timer) {
    app_timer_cancel(s_draw_timer);
    s_draw_timer = NULL;
  }
  layer_destroy(s_sky_layer);
  s_sky_layer = NULL;
}

static void init(void) {
  memset(&g_app, 0, sizeof(g_app));
  g_app.show_below_horizon = true;
  g_app.show_faint_stars = true;
  g_app.show_bright_stars = true;
  g_app.show_planets = true;
  g_app.show_sun = true;
  g_app.show_moon = true;
  g_app.show_dwarfs = true;
  g_app.show_asteroids = true;
  g_app.show_sats = false;
  g_app.show_gps = false;
  g_app.show_lagrange = false;
  g_app.show_clusters = true;
  g_app.show_galaxies = true;
  g_app.show_nebulae = true;
  g_app.show_constellations = true;
  g_app.show_asterisms = true;
  g_app.show_cardinals = true;
  g_app.show_heading = true;
  g_app.show_ecliptic = true;
#if APP_TOUCH_ONLY
  g_app.touch_look = true;
#else
  g_app.touch_look = false;
#endif
  catalog_build_named_index();
  asterism_init();
  load_persisted_location();
  menu_init();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
    .appear = window_appear,
    .disappear = window_disappear,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(1536, 128);

  apply_look_input_mode();
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  s_draw_timer = app_timer_register(50, draw_timer_cb, NULL);

  request_phone_update();
}

static void deinit(void) {
  if (s_draw_timer) {
    app_timer_cancel(s_draw_timer);
    s_draw_timer = NULL;
  }
  tick_timer_service_unsubscribe();
#ifdef PBL_TOUCH
  touch_listen(false);
#endif
  sensors_disable();
#if APP_HAS_LOOK_SENSORS
  if (s_cal_done_timer) {
    app_timer_cancel(s_cal_done_timer);
    s_cal_done_timer = NULL;
  }
  if (s_cal_visible && s_cal_window) {
    window_stack_remove(compass_calibration_window_get_window(s_cal_window), false);
    s_cal_visible = false;
  }
  compass_calibration_window_destroy(s_cal_window);
  s_cal_window = NULL;
#endif
  menu_deinit();
  light_enable(false);
#ifdef PBL_PLATFORM_EMERY
  light_set_system_color();
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
