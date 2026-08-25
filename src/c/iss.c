#include "iss.h"
#include "starwatch.h"

#include <pebble.h>
#include <string.h>
#include <time.h>

#define GM 398600.4418f
#define RE 6378.137f
#define J2 1.08262668e-3f
#define WGS_E2 0.00669437999f
#define UNIX_J2000 946728000

typedef struct {
  int32_t epoch_unix;
  float inc_deg;
  float raan_deg;
  float ecc;
  float argp_deg;
  float m0_deg;
  float n_revday;
  bool valid;
} IssTle;

static IssTle s_tle;
static int s_tle_index;
static float s_az_deg;
static float s_alt_deg;
static bool s_horiz_ok;

static float wrap360(float deg) {
  int32_t n;
  if (deg >= 0.0f && deg < 360.0f) {
    return deg;
  }
  n = (int32_t)(deg / 360.0f);
  deg -= (float)n * 360.0f;
  if (deg < 0.0f) {
    deg += 360.0f;
  } else if (deg >= 360.0f) {
    deg -= 360.0f;
  }
  return deg;
}

static int32_t deg_to_trig(float deg) {
  deg = wrap360(deg);
  return (int32_t)((deg * (float)TRIG_MAX_ANGLE) / 360.0f);
}

static float lookup_sin(float deg) {
  return ((float)sin_lookup(deg_to_trig(deg))) / (float)TRIG_MAX_RATIO;
}

static float lookup_cos(float deg) {
  return ((float)cos_lookup(deg_to_trig(deg))) / (float)TRIG_MAX_RATIO;
}

static float lookup_atan2(float y, float x) {
  float ay = (y >= 0.0f) ? y : -y;
  float ax = (x >= 0.0f) ? x : -x;
  float m = (ay > ax) ? ay : ax;
  int32_t ang;
  float deg;
  if (m < 1e-12f) {
    return 0.0f;
  }
  /* atan2_lookup takes int16; scale so the larger axis fits.
     Convert via float so azimuth/altitude are not quantized to 1°. */
  ang = atan2_lookup((int16_t)(y * (30000.0f / m)),
                     (int16_t)(x * (30000.0f / m)));
  deg = ((float)ang * 360.0f) / (float)TRIG_MAX_ANGLE;
  if (deg > 180.0f) {
    deg -= 360.0f;
  }
  return deg;
}

static float fast_sqrt(float x) {
  int i;
  float g;
  if (x <= 0.0f) {
    return 0.0f;
  }
  g = x;
  for (i = 0; i < 8; i++) {
    g = 0.5f * (g + x / g);
  }
  return g;
}

static float cube_root(float x) {
  int i;
  float g = 7000.0f;
  if (x <= 0.0f) {
    return 0.0f;
  }
  for (i = 0; i < 12; i++) {
    g = (2.0f * g + x / (g * g)) / 3.0f;
  }
  return g;
}

static int32_t read_i32(const uint8_t *p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static uint16_t read_u16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

void iss_set_tle(const uint8_t *data, int length) {
  int off = 0;
  int index = 0;
  if (!data || length < 20) {
    return;
  }
  if (length >= 22) {
    index = (int)(data[0] | (data[1] << 8));
    off = 2;
    if (length - off < 20) {
      return;
    }
  }
  if (index < 0 || index >= SAT_COUNT) {
    return;
  }
  s_tle_index = index;
  s_tle.epoch_unix = read_i32(data + off);
  s_tle.inc_deg = read_u16(data + off + 4) / 100.0f;
  s_tle.raan_deg = read_u16(data + off + 6) / 100.0f;
  s_tle.ecc = read_i32(data + off + 8) / 1000000.0f;
  s_tle.argp_deg = read_u16(data + off + 12) / 100.0f;
  s_tle.m0_deg = read_u16(data + off + 14) / 100.0f;
  s_tle.n_revday = read_i32(data + off + 16) / 1000000.0f;
  s_tle.valid = s_tle.n_revday > 1.8f && s_tle.n_revday < 17.0f;
}

bool iss_ready(void) {
  return s_tle.valid;
}

bool iss_ready_index(int index) {
  return s_tle.valid && s_tle_index == index;
}

bool iss_horiz(float *az_deg, float *alt_deg) {
  if (!s_horiz_ok) {
    return false;
  }
  *az_deg = s_az_deg;
  *alt_deg = s_alt_deg;
  return true;
}

void iss_update(void) {
  float dt_days;
  float n;
  float n_rad;
  float a;
  float e;
  float i_deg;
  float cosi;
  float sini;
  float ecc2;
  float pfac;
  float raan;
  float argp;
  float m_deg;
  float e_deg;
  int k;
  float sin_e;
  float cos_e;
  float xv;
  float yv;
  float v_deg;
  float r;
  float u_deg;
  float x;
  float y;
  float z;
  float gmst;
  float x_ecef;
  float y_ecef;
  float z_ecef;
  float sin_lat;
  float cos_lat;
  float sin_lon;
  float cos_lon;
  float n_pr;
  float obs_x;
  float obs_y;
  float obs_z;
  float rx;
  float ry;
  float rz;
  float east;
  float north;
  float up;
  float horiz;
  time_t now;
  uint16_t ms;
  float frac;
  int32_t sec_j2000;
  int32_t days_j2000;
  int32_t sod;

  s_horiz_ok = false;
  if (!s_tle.valid) {
    return;
  }
  time_ms(&now, &ms);
  frac = (float)ms / 1000.0f;
  {
    int32_t dt_sec = (int32_t)now - s_tle.epoch_unix;
    int32_t dt_day = dt_sec / 86400;
    int32_t dt_sod = dt_sec % 86400;
    if (dt_sod < 0) {
      dt_day -= 1;
      dt_sod += 86400;
    }
    dt_days = (float)dt_day + ((float)dt_sod + frac) / 86400.0f;
  }
  n = s_tle.n_revday;
  n_rad = n * 2.0f * 3.14159265f / 86400.0f;
  a = cube_root(GM / (n_rad * n_rad));
  e = s_tle.ecc;
  i_deg = s_tle.inc_deg;
  cosi = lookup_cos(i_deg);
  sini = lookup_sin(i_deg);
  ecc2 = 1.0f - e * e;
  if (ecc2 < 0.0001f) {
    ecc2 = 0.0001f;
  }
  pfac = (RE / a) * (RE / a) / (ecc2 * ecc2);
  raan = wrap360(s_tle.raan_deg + (-1.5f * n * 360.0f * J2 * pfac * cosi) * dt_days);
  argp = wrap360(s_tle.argp_deg +
                 (0.75f * n * 360.0f * J2 * pfac * (5.0f * cosi * cosi - 1.0f)) * dt_days);
  m_deg = wrap360(s_tle.m0_deg + n * 360.0f * dt_days);
  e_deg = m_deg;
  for (k = 0; k < 8; k++) {
    sin_e = lookup_sin(e_deg);
    cos_e = lookup_cos(e_deg);
    e_deg = e_deg - ((e_deg * 0.017453292f - e * sin_e - m_deg * 0.017453292f) /
                     (1.0f - e * cos_e)) * 57.29578f;
  }
  sin_e = lookup_sin(e_deg);
  cos_e = lookup_cos(e_deg);
  xv = cos_e - e;
  yv = fast_sqrt(ecc2) * sin_e;
  v_deg = lookup_atan2(yv, xv);
  r = a * fast_sqrt(xv * xv + yv * yv);
  u_deg = v_deg + argp;
  x = r * (lookup_cos(raan) * lookup_cos(u_deg) - lookup_sin(raan) * lookup_sin(u_deg) * cosi);
  y = r * (lookup_sin(raan) * lookup_cos(u_deg) + lookup_cos(raan) * lookup_sin(u_deg) * cosi);
  z = r * (lookup_sin(u_deg) * sini);

  sec_j2000 = (int32_t)now - UNIX_J2000;
  days_j2000 = sec_j2000 / 86400;
  sod = sec_j2000 % 86400;
  if (sod < 0) {
    days_j2000 -= 1;
    sod += 86400;
  }
  gmst = wrap360(280.46061837f + 0.98564736629f * (float)days_j2000 +
                 360.98564736629f * (((float)sod + frac) / 86400.0f));
  x_ecef = x * lookup_cos(gmst) + y * lookup_sin(gmst);
  y_ecef = -x * lookup_sin(gmst) + y * lookup_cos(gmst);
  z_ecef = z;

  sin_lat = lookup_sin(g_app.lat_deg);
  cos_lat = lookup_cos(g_app.lat_deg);
  sin_lon = lookup_sin(g_app.lon_deg);
  cos_lon = lookup_cos(g_app.lon_deg);
  n_pr = RE / fast_sqrt(1.0f - WGS_E2 * sin_lat * sin_lat);
  obs_x = n_pr * cos_lat * cos_lon;
  obs_y = n_pr * cos_lat * sin_lon;
  obs_z = n_pr * (1.0f - WGS_E2) * sin_lat;
  rx = x_ecef - obs_x;
  ry = y_ecef - obs_y;
  rz = z_ecef - obs_z;
  east = -sin_lon * rx + cos_lon * ry;
  north = -sin_lat * cos_lon * rx - sin_lat * sin_lon * ry + cos_lat * rz;
  up = cos_lat * cos_lon * rx + cos_lat * sin_lon * ry + sin_lat * rz;
  horiz = fast_sqrt(east * east + north * north);
  s_az_deg = wrap360(lookup_atan2(east, north));
  s_alt_deg = lookup_atan2(up, horiz);
  s_horiz_ok = true;
}
