#include "lagrange.h"
#include "starwatch.h"

#include <pebble.h>

#define OBL 23.44f
#define NEP_RA 270.0f
#define NEP_DEC 66.56f

typedef struct {
  float x;
  float y;
  float z;
} V3;

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

static V3 v3(float x, float y, float z) {
  V3 o;
  o.x = x;
  o.y = y;
  o.z = z;
  return o;
}

static float dot(V3 a, V3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static V3 cross(V3 a, V3 b) {
  return v3(a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
}

static V3 scale(V3 a, float s) {
  return v3(a.x * s, a.y * s, a.z * s);
}

static V3 add3(V3 a, V3 b) {
  return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static V3 norm(V3 a) {
  float len = fast_sqrt(dot(a, a));
  if (len < 1e-8f) {
    return a;
  }
  return scale(a, 1.0f / len);
}

static V3 eq_dir(float ra, float dec) {
  float cd = lookup_cos(dec);
  return v3(cd * lookup_cos(ra), cd * lookup_sin(ra), lookup_sin(dec));
}

static void dir_eq(V3 d, float *ra, float *dec) {
  float hyp;
  d = norm(d);
  hyp = fast_sqrt(d.x * d.x + d.y * d.y);
  *dec = lookup_atan2(d.z, hyp);
  *ra = wrap360(lookup_atan2(d.y, d.x));
}

static V3 rotate_nep(V3 v, float deg) {
  V3 k = eq_dir(NEP_RA, NEP_DEC);
  float c = lookup_cos(deg);
  float s = lookup_sin(deg);
  V3 kxv = cross(k, v);
  float kdot = dot(k, v);
  return add3(add3(scale(v, c), scale(kxv, s)),
              scale(k, kdot * (1.0f - c)));
}

static const char *const LAG_NAMES[LAGRANGE_COUNT] = {
  "Sun-Earth L1", "Sun-Earth L2", "Sun-Earth L3",
  "Sun-Earth L4", "Sun-Earth L5",
  "Earth-Moon L1", "Earth-Moon L2", "Earth-Moon L3",
  "Earth-Moon L4", "Earth-Moon L5"
};

/* Angular diameter of a typical libration/halo region as seen from Earth. */
static const float LAG_SIZE[LAGRANGE_COUNT] = {
  8.0f, 8.0f, 0.8f, 12.0f, 12.0f,
  5.0f, 5.0f, 4.0f, 8.0f, 8.0f
};

int lagrange_count(void) {
  return LAGRANGE_COUNT;
}

const char *lagrange_name(int index) {
  if (index < 0 || index >= LAGRANGE_COUNT) {
    return "Lagrange";
  }
  return LAG_NAMES[index];
}

float lagrange_size_deg(int index) {
  if (index < 0 || index >= LAGRANGE_COUNT) {
    return 2.0f;
  }
  return LAG_SIZE[index];
}

bool lagrange_ready(void) {
  return g_app.planets_valid;
}

void lagrange_equatorial(int index, float *ra_deg, float *dec_deg) {
  V3 sun;
  V3 moon;
  V3 d;
  if (!ra_deg || !dec_deg) {
    return;
  }
  *ra_deg = 0.0f;
  *dec_deg = 0.0f;
  if (!g_app.planets_valid || index < 0 || index >= LAGRANGE_COUNT) {
    return;
  }
  sun = eq_dir(g_app.planet_ra_deg[BODY_SUN], g_app.planet_dec_deg[BODY_SUN]);
  moon = eq_dir(g_app.planet_ra_deg[BODY_MOON], g_app.planet_dec_deg[BODY_MOON]);
  switch (index) {
    case 0: /* SE L1: toward the Sun */
    case 2: /* SE L3: behind the Sun, same sky line */
      d = sun;
      break;
    case 1: /* SE L2: anti-sun */
      d = scale(sun, -1.0f);
      break;
    case 3:
      d = rotate_nep(sun, 60.0f);
      break;
    case 4:
      d = rotate_nep(sun, -60.0f);
      break;
    case 5: /* EM L1 */
    case 6: /* EM L2: same line as the Moon from Earth */
      d = moon;
      break;
    case 7:
      d = scale(moon, -1.0f);
      break;
    case 8:
      d = rotate_nep(moon, 60.0f);
      break;
    default:
      d = rotate_nep(moon, -60.0f);
      break;
  }
  dir_eq(d, ra_deg, dec_deg);
  (void)OBL;
}
