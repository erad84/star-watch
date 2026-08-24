#include "starwatch.h"
#include "draw.h"
#include "sky.h"
#include "catalog.h"
#include "asterism.h"
#include "iss.h"
#include "menu.h"
#include "compass_calibration_window.h"

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

const char *const SAT_NAMES[SAT_COUNT] = {
  "ISS"
};

static Window *s_window;
static Layer *s_sky_layer;
static AppTimer *s_draw_timer;
static AppTimer *s_cal_done_timer;
static bool s_did_refresh;
static CompassCalibrationWindow *s_cal_window;
static bool s_cal_visible;
static bool s_cal_dismissed;
static bool s_ring_complete;
static CompassStatus s_compass_status;
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
static uint32_t s_compass_ms;

#define LOOK_TICK_MS 20
#define ALT_LP_ALPHA 0.28f
#define ALT_CHASE_ALPHA 0.24f
#define LOOK_EPS 0.03f
#define RATE_DECAY 0.90f
#define COMPASS_CORR_SLOW 0.03f
#define COMPASS_CORR_TURN 0.22f

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
    d = circ_delta(g_app.look_az_deg, s_az_fused);
    if (d > LOOK_EPS || d < -LOOK_EPS) {
      g_app.look_az_deg = s_az_fused;
      moved = true;
    }
  }
  if (s_alt_filt_ready) {
    d = s_alt_target - g_app.look_alt_deg;
    a = chase_alpha(absf(d), ALT_CHASE_ALPHA, 0.36f, 0.50f);
    if (d > LOOK_EPS || d < -LOOK_EPS) {
      g_app.look_alt_deg += a * d;
      moved = true;
    }
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
  if (s_az_filt_ready && absf(s_az_rate_dps) > 0.8f) {
    return true;
  }
  if (s_az_filt_ready && s_compass_ready &&
      absf(circ_delta(s_az_fused, s_compass_az)) > LOOK_EPS) {
    return true;
  }
  if (s_alt_filt_ready && absf(s_alt_target - g_app.look_alt_deg) > LOOK_EPS) {
    return true;
  }
  return false;
}

static bool apply_look_az(float compass_az) {
  uint32_t t = now_ms();
  float d;
  float dt;
  compass_az = wrap360f(compass_az);
  if (!s_az_filt_ready) {
    s_az_fused = compass_az;
    s_compass_az = compass_az;
    g_app.look_az_deg = compass_az;
    s_az_filt_ready = true;
    s_compass_ready = true;
    s_compass_ms = t;
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
  if (alt_raw > 90.0f) {
    alt_raw = 90.0f;
  } else if (alt_raw < -90.0f) {
    alt_raw = -90.0f;
  }
  if (!s_alt_filt_ready) {
    s_alt_lp = alt_raw;
    s_alt_target = alt_raw;
    g_app.look_alt_deg = alt_raw;
    s_alt_filt_ready = true;
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
  g_app.show_sats = persist_flag(PERSIST_SATS, true);
  g_app.show_clusters = persist_flag(PERSIST_CLUSTERS, true);
  g_app.show_galaxies = persist_flag(PERSIST_GALAXIES, true);
  g_app.show_nebulae = persist_flag(PERSIST_NEBULAE, true);
  g_app.show_constellations = persist_flag(PERSIST_CONSTELLS, true);
  g_app.show_asterisms = persist_flag(PERSIST_ASTERISMS, true);
  g_app.show_cardinals = persist_flag(PERSIST_CARDINALS, true);
  g_app.show_heading = persist_flag(PERSIST_HEADING, true);
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
  bool iss = g_app.target_mode == TARGET_NAMED &&
             g_app.target_kind == TARGET_KIND_SAT;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_TrackISS, iss ? 1 : 0);
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
  apply_packed_eq(iss_t, &g_app.iss_ra_deg, &g_app.iss_dec_deg, SAT_COUNT,
                  &g_app.iss_valid);
  if (tle_t && tle_t->type == TUPLE_BYTE_ARRAY) {
    iss_set_tle(tle_t->value->data, tle_t->length);
  }
  request_draw();
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  (void)context;
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped %d", (int)reason);
}

static float altitude_from_accel(int32_t y, int32_t z) {
  float deg = trig_to_deg(atan2_lookup(z, -y));
  if (deg > 180.0f) {
    deg -= 360.0f;
  }
  if (deg > 90.0f) {
    deg = 90.0f;
  } else if (deg < -90.0f) {
    deg = -90.0f;
  }
  return deg;
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

static void present_calibration(bool force) {
  BatteryChargeState bat;
  if (s_cal_visible) {
    if (force && s_cal_window) {
      compass_calibration_window_reset(s_cal_window);
    }
    return;
  }
  if (!force && (s_cal_dismissed || s_ring_complete)) {
    return;
  }
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
  static int last_status = -1;
  const bool usable = (heading.compass_status == CompassStatusCalibrating ||
                       heading.compass_status == CompassStatusCalibrated);
  if ((int)heading.compass_status != last_status) {
    last_status = (int)heading.compass_status;
    APP_LOG(APP_LOG_LEVEL_INFO, "compass status %d", last_status);
  }
  if (g_app.compass_ok != usable) {
    g_app.compass_ok = usable;
    request_draw();
  }
  s_compass_status = heading.compass_status;
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

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  sky_set_bounds(bounds.size);

  s_sky_layer = layer_create(bounds);
  layer_set_update_proc(s_sky_layer, draw_sky_update);
  layer_add_child(root, s_sky_layer);
  APP_LOG(APP_LOG_LEVEL_INFO, "Star Watch window loaded");
}

static bool iss_needs_anim(void) {
  if (!iss_ready()) {
    return false;
  }
  if (g_app.show_sats && g_app.sky_mode == SKY_STARS) {
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
  chasing = chase_look();
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
  g_app.show_sats = true;
  g_app.show_clusters = true;
  g_app.show_galaxies = true;
  g_app.show_nebulae = true;
  g_app.show_constellations = true;
  g_app.show_asterisms = true;
  g_app.show_cardinals = true;
  g_app.show_heading = true;
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
  });
  window_stack_push(s_window, true);
  APP_LOG(APP_LOG_LEVEL_INFO, "Star Watch init");

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(1024, 128);

  accel_raw_data_service_subscribe(1, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
  compass_service_subscribe(compass_handler);
  compass_service_set_heading_filter(0);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  s_draw_timer = app_timer_register(50, draw_timer_cb, NULL);

  request_phone_update();
}

static void deinit(void) {
  if (s_draw_timer) {
    app_timer_cancel(s_draw_timer);
    s_draw_timer = NULL;
  }
  if (s_cal_done_timer) {
    app_timer_cancel(s_cal_done_timer);
    s_cal_done_timer = NULL;
  }
  tick_timer_service_unsubscribe();
  compass_service_unsubscribe();
  accel_data_service_unsubscribe();
  if (s_cal_visible && s_cal_window) {
    window_stack_remove(compass_calibration_window_get_window(s_cal_window), false);
    s_cal_visible = false;
  }
  compass_calibration_window_destroy(s_cal_window);
  s_cal_window = NULL;
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
