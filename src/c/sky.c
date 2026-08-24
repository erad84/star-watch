#include "sky.h"
#include "catalog.h"
#include "starwatch.h"

#define SKY_MAX_STARS 904
#define J2000_UNIX 946728000
/* tan(25°) for 50° FOV */
#define TAN_HALF_FOV 0.46630765815f

typedef struct {
  float e;
  float n;
  float u;
} Vec3;

typedef struct {
  int16_t e;
  int16_t n;
  int16_t u;
} Vec3i;

#define VEC_SCALE 16384.0f
#define VEC_UNSCALE (1.0f / 16384.0f)

static GSize s_bounds;
static float s_lat_deg;
static float s_lon_deg;
static float s_lst_hours;
static Vec3 s_look;
static Vec3 s_right;
static Vec3 s_up;
static float s_focal;
static int16_t s_look_az;
static int16_t s_look_alt;
static float s_look_az_f;
static float s_look_alt_f;
static bool s_look_ready;
static float s_sin_lat;
static float s_cos_lat = 1.0f;
static int16_t s_star_az[SKY_MAX_STARS];
static int16_t s_star_alt[SKY_MAX_STARS];
#ifndef PBL_PLATFORM_FLINT
static Vec3i s_star_enu[SKY_MAX_STARS];
#endif
static Vec3i s_horizon[SKY_HORIZON_STEPS + 1];
static bool s_horizon_ready;
static int s_stars_ready;

static float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

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
  int32_t iy = (int32_t)(y * 1024.0f);
  int32_t ix = (int32_t)(x * 1024.0f);
  int32_t deg = TRIGANGLE_TO_DEG(atan2_lookup(iy, ix));
  if (deg > 180) {
    deg -= 360;
  }
  return (float)deg;
}

static float fast_sqrt(float x) {
  int i;
  float g;
  if (x <= 0.0f) {
    return 0.0f;
  }
  g = x;
  for (i = 0; i < 4; i++) {
    g = 0.5f * (g + x / g);
  }
  return g;
}

static Vec3 vec3(float e, float n, float u) {
  Vec3 v;
  v.e = e;
  v.n = n;
  v.u = u;
  return v;
}

static int16_t pack_comp(float v) {
  int32_t n = (int32_t)(v * VEC_SCALE);
  if (n > 32767) {
    n = 32767;
  } else if (n < -32767) {
    n = -32767;
  }
  return (int16_t)n;
}

static Vec3i pack_vec(Vec3 v) {
  Vec3i p;
  p.e = pack_comp(v.e);
  p.n = pack_comp(v.n);
  p.u = pack_comp(v.u);
  return p;
}

static Vec3 unpack_vec(Vec3i p) {
  return vec3((float)p.e * VEC_UNSCALE,
              (float)p.n * VEC_UNSCALE,
              (float)p.u * VEC_UNSCALE);
}

static float dot3(Vec3 a, Vec3 b) {
  return a.e * b.e + a.n * b.n + a.u * b.u;
}

static Vec3 cross3(Vec3 a, Vec3 b) {
  return vec3(a.n * b.u - a.u * b.n,
              a.u * b.e - a.e * b.u,
              a.e * b.n - a.n * b.e);
}

static Vec3 norm3(Vec3 v) {
  float len = fast_sqrt(dot3(v, v));
  if (len < 1e-6f) {
    return v;
  }
  return vec3(v.e / len, v.n / len, v.u / len);
}

static void horiz_from_equatorial(float ra_deg, float dec_deg,
                                  float *az_deg, float *alt_deg) {
  float ha = s_lst_hours * 15.0f - ra_deg;
  float sin_dec = lookup_sin(dec_deg);
  float cos_dec = lookup_cos(dec_deg);
  float sin_lat = s_sin_lat;
  float cos_lat = s_cos_lat;
  float sin_ha = lookup_sin(ha);
  float cos_ha = lookup_cos(ha);
  float sin_alt = sin_dec * sin_lat + cos_dec * cos_lat * cos_ha;
  float cos_alt;
  float alt;
  float az;

  sin_alt = clampf(sin_alt, -1.0f, 1.0f);
  cos_alt = fast_sqrt(1.0f - sin_alt * sin_alt);
  alt = lookup_atan2(sin_alt, cos_alt);
  if (cos_alt < 1e-5f) {
    az = 0.0f;
  } else {
    float sin_az = -sin_ha * cos_dec / cos_alt;
    float cos_az = (sin_dec - sin_alt * sin_lat) / (cos_alt * cos_lat);
    az = lookup_atan2(sin_az, cos_az);
  }
  *alt_deg = alt;
  *az_deg = wrap360(az);
}

static Vec3 enu_from_horiz(float az_deg, float alt_deg) {
  float c_alt = lookup_cos(alt_deg);
  return vec3(lookup_sin(az_deg) * c_alt, lookup_cos(az_deg) * c_alt, lookup_sin(alt_deg));
}

void sky_set_bounds(GSize size) {
  s_bounds = size;
  s_focal = ((float)size.w * 0.5f) / TAN_HALF_FOV;
  s_look_ready = false;
}

int sky_ang_radius_px(float diam_deg) {
  float half = diam_deg * 0.5f;
  float c = lookup_cos(half);
  float t;
  int r;
  if (c < 0.05f) {
    c = 0.05f;
  }
  t = lookup_sin(half) / c;
  r = (int)(s_focal * t + 0.5f);
  if (r < 2) {
    r = 2;
  }
  return r;
}

void sky_set_observer(float lat_deg, float lon_deg) {
  s_lat_deg = lat_deg;
  s_lon_deg = lon_deg;
  s_sin_lat = lookup_sin(lat_deg);
  s_cos_lat = lookup_cos(lat_deg);
}

void sky_set_look(float az_deg, float alt_deg) {
  Vec3 right;
  int16_t az;
  int16_t alt;
  if (s_look_ready) {
    float daz = az_deg - s_look_az_f;
    float dalt = alt_deg - s_look_alt_f;
    if (daz > 180.0f) {
      daz -= 360.0f;
    } else if (daz < -180.0f) {
      daz += 360.0f;
    }
    if (daz > -0.04f && daz < 0.04f && dalt > -0.04f && dalt < 0.04f) {
      return;
    }
  }
  az = (int16_t)(az_deg + (az_deg >= 0.0f ? 0.5f : -0.5f));
  alt = (int16_t)(alt_deg + (alt_deg >= 0.0f ? 0.5f : -0.5f));
  s_look_az = az;
  s_look_alt = alt;
  s_look_az_f = az_deg;
  s_look_alt_f = alt_deg;
  s_look_ready = true;
  s_look = enu_from_horiz(az_deg, alt_deg);
  right = cross3(s_look, vec3(0.0f, 0.0f, 1.0f));
  if (dot3(right, right) < 0.0025f) {
    right = vec3(lookup_cos(az_deg), -lookup_sin(az_deg), 0.0f);
    s_up = vec3(lookup_sin(az_deg), lookup_cos(az_deg), 0.0f);
    if (s_look.u < 0.0f) {
      s_up.e = -s_up.e;
      s_up.n = -s_up.n;
    }
  } else {
    right = norm3(right);
    s_up = cross3(right, s_look);
  }
  s_right = norm3(right);
  s_up = norm3(s_up);
}

void sky_update_time(void) {
  time_t now = time(NULL);
  float days = ((float)((int32_t)now - J2000_UNIX)) / 86400.0f;
  float gmst = 18.697374558f + 24.06570982441908f * days;
  int32_t wraps = (int32_t)(gmst / 24.0f);
  gmst = gmst - (float)wraps * 24.0f;
  if (gmst < 0.0f) {
    gmst += 24.0f;
  }
  s_lst_hours = gmst + s_lon_deg / 15.0f;
  while (s_lst_hours < 0.0f) {
    s_lst_hours += 24.0f;
  }
  while (s_lst_hours >= 24.0f) {
    s_lst_hours -= 24.0f;
  }
}

void sky_refresh_stars(void) {
  int n = catalog_star_count();
  int i;
  if (n > SKY_MAX_STARS) {
    n = SKY_MAX_STARS;
  }
  for (i = 0; i < n; i++) {
    const PackedStar *s = catalog_star(i);
    float az;
    float alt;
    horiz_from_equatorial(catalog_ra_deg(s), catalog_dec_deg(s), &az, &alt);
    s_star_az[i] = (int16_t)(az + 0.5f);
    s_star_alt[i] = (int16_t)(alt + (alt >= 0.0f ? 0.5f : -0.5f));
#ifndef PBL_PLATFORM_FLINT
    s_star_enu[i] = pack_vec(enu_from_horiz(az, alt));
#endif
  }
  s_stars_ready = 1;
}

static bool project_vec(Vec3 obj, int16_t *px, int16_t *py) {
  float z = dot3(obj, s_look);
  float x;
  float y;
  float sx;
  float sy;
  if (z < 0.12f) {
    return false;
  }
  x = dot3(obj, s_right);
  y = dot3(obj, s_up);
  sx = ((float)s_bounds.w * 0.5f) + (x / z) * s_focal;
  sy = ((float)s_bounds.h * 0.5f) - (y / z) * s_focal;
  if (sx < -20.0f || sy < -20.0f ||
      sx > (float)s_bounds.w + 20.0f || sy > (float)s_bounds.h + 20.0f) {
    return false;
  }
  *px = (int16_t)(sx + 0.5f);
  *py = (int16_t)(sy + 0.5f);
  return true;
}

bool sky_project_equatorial(float ra_deg, float dec_deg, int16_t *px, int16_t *py) {
  float az;
  float alt;
  horiz_from_equatorial(ra_deg, dec_deg, &az, &alt);
  return project_vec(enu_from_horiz(az, alt), px, py);
}

float sky_alt_equatorial(float ra_deg, float dec_deg) {
  float az;
  float alt;
  horiz_from_equatorial(ra_deg, dec_deg, &az, &alt);
  return alt;
}

bool sky_project_horiz(float az_deg, float alt_deg, int16_t *px, int16_t *py) {
  return project_vec(enu_from_horiz(az_deg, alt_deg), px, py);
}

bool sky_star_on_screen(int index, int16_t *px, int16_t *py) {
  int daz;
  int dalt;
  if (!s_stars_ready || index < 0 || index >= catalog_star_count() ||
      index >= SKY_MAX_STARS) {
    return false;
  }
  daz = s_star_az[index] - s_look_az;
  if (daz < 0) {
    daz = -daz;
  }
  if (daz > 180) {
    daz = 360 - daz;
  }
  dalt = s_star_alt[index] - s_look_alt;
  if (dalt < 0) {
    dalt = -dalt;
  }
  if (daz > 70 || dalt > 70) {
    return false;
  }
#ifdef PBL_PLATFORM_FLINT
  return project_vec(enu_from_horiz((float)s_star_az[index], (float)s_star_alt[index]), px, py);
#else
  return project_vec(unpack_vec(s_star_enu[index]), px, py);
#endif
}

bool sky_horizon_point(int i, int16_t *px, int16_t *py) {
  if (!s_horizon_ready) {
    int k;
    for (k = 0; k <= SKY_HORIZON_STEPS; k++) {
      float az = (360.0f * (float)k) / (float)SKY_HORIZON_STEPS;
      s_horizon[k] = pack_vec(enu_from_horiz(az, 0.0f));
    }
    s_horizon_ready = true;
  }
  if (i < 0 || i > SKY_HORIZON_STEPS) {
    return false;
  }
  return project_vec(unpack_vec(s_horizon[i]), px, py);
}

int16_t sky_star_alt_deg(int index) {
  if (!s_stars_ready || index < 0 || index >= SKY_MAX_STARS) {
    return 0;
  }
  return s_star_alt[index];
}

static void aim_to_edge(float dx, float dy, int16_t *px, int16_t *py) {
  const float cx = (float)s_bounds.w * 0.5f;
  const float cy = (float)s_bounds.h * 0.5f;
  const float x0 = 4.0f;
  const float y0 = 4.0f;
  const float x1 = (float)s_bounds.w - 5.0f;
  const float y1 = (float)s_bounds.h - 5.0f;
  float t = 1e6f;
  float tt;
  if (dx > 1e-5f) {
    tt = (x1 - cx) / dx;
    if (tt > 0.0f && tt < t) {
      t = tt;
    }
  } else if (dx < -1e-5f) {
    tt = (x0 - cx) / dx;
    if (tt > 0.0f && tt < t) {
      t = tt;
    }
  }
  if (dy > 1e-5f) {
    tt = (y1 - cy) / dy;
    if (tt > 0.0f && tt < t) {
      t = tt;
    }
  } else if (dy < -1e-5f) {
    tt = (y0 - cy) / dy;
    if (tt > 0.0f && tt < t) {
      t = tt;
    }
  }
  if (t > 1e5f) {
    t = 1.0f;
  }
  *px = (int16_t)(cx + t * dx + 0.5f);
  *py = (int16_t)(cy + t * dy + 0.5f);
}

void sky_aim_from_horiz(float az_deg, float alt_deg, int16_t *px, int16_t *py, bool *on_screen) {
  Vec3 obj;
  float z;
  float x;
  float y;
  float cx;
  float cy;
  float sx;
  float sy;

  obj = enu_from_horiz(az_deg, alt_deg);
  z = dot3(obj, s_look);
  x = dot3(obj, s_right);
  y = dot3(obj, s_up);
  cx = (float)s_bounds.w * 0.5f;
  cy = (float)s_bounds.h * 0.5f;

  if (z > 0.12f) {
    sx = cx + (x / z) * s_focal;
    sy = cy - (y / z) * s_focal;
    if (sx >= 0.0f && sy >= 0.0f &&
        sx < (float)s_bounds.w && sy < (float)s_bounds.h) {
      *px = (int16_t)(sx + 0.5f);
      *py = (int16_t)(sy + 0.5f);
      if (on_screen) {
        *on_screen = true;
      }
      return;
    }
    aim_to_edge(sx - cx, sy - cy, px, py);
    if (on_screen) {
      *on_screen = false;
    }
    return;
  }

  if (x * x + y * y < 1e-6f) {
    aim_to_edge(0.0f, 1.0f, px, py);
  } else {
    aim_to_edge(x, -y, px, py);
  }
  if (on_screen) {
    *on_screen = false;
  }
}

void sky_aim_equatorial(float ra_deg, float dec_deg, int16_t *px, int16_t *py, bool *on_screen) {
  float az;
  float alt;
  horiz_from_equatorial(ra_deg, dec_deg, &az, &alt);
  sky_aim_from_horiz(az, alt, px, py, on_screen);
}
