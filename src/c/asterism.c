#include "asterism.h"
#include "catalog.h"

#include <stdint.h>

#define ASTERISM_COUNT 30
#define ASTERISM_MAX_VERTS 256

typedef struct {
  const char *name;
  float ra_deg;
  float dec_deg;
} AsterismVertDef;

typedef struct {
  const char *title;
  const AsterismVertDef *verts;
  const uint8_t *segs;
  uint8_t n_vert;
  uint8_t n_seg;
} AsterismDef;

static const AsterismVertDef V_BIG_DIPPER[] = {
  {"Dubhe", 0, 0}, {"Merak", 0, 0}, {"Phecda", 0, 0}, {"Megrez", 0, 0},
  {"Alioth", 0, 0}, {"Mizar", 0, 0}, {"Alkaid", 0, 0}
};
static const uint8_t S_BIG_DIPPER[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 0, 3};

static const AsterismVertDef V_BELT[] = {
  {"Mintaka", 0, 0}, {"Alnilam", 0, 0}, {"Alnitak", 0, 0}
};
static const uint8_t S_BELT[] = {0, 1, 1, 2};

static const AsterismVertDef V_SUMMER_TRI[] = {
  {"Vega", 0, 0}, {"Deneb", 0, 0}, {"Altair", 0, 0}
};
static const uint8_t S_SUMMER_TRI[] = {0, 1, 1, 2, 2, 0};

static const AsterismVertDef V_S_CROSS[] = {
  {"Acrux", 0, 0}, {"Gacrux", 0, 0}, {"Mimosa", 0, 0}, {NULL, 183.786f, -58.749f}
};
static const uint8_t S_S_CROSS[] = {0, 1, 2, 3};

static const AsterismVertDef V_WINTER_TRI[] = {
  {"Betelgeuse", 0, 0}, {"Procyon", 0, 0}, {"Sirius", 0, 0}
};
static const uint8_t S_WINTER_TRI[] = {0, 1, 1, 2, 2, 0};

static const AsterismVertDef V_LITTLE_DIPPER[] = {
  {"Polaris", 0, 0},
  {NULL, 263.054f, 86.586f},
  {NULL, 251.493f, 82.037f},
  {NULL, 236.015f, 77.794f},
  {NULL, 244.376f, 75.755f},
  {"Pherkad", 0, 0},
  {"Kochab", 0, 0}
};
static const uint8_t S_LITTLE_DIPPER[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 3};

static const AsterismVertDef V_SQUARE[] = {
  {"Markab", 0, 0}, {"Scheat", 0, 0}, {"Alpheratz", 0, 0}, {"Algenib", 0, 0}
};
static const uint8_t S_SQUARE[] = {0, 1, 1, 2, 2, 3, 3, 0};

static const AsterismVertDef V_N_CROSS[] = {
  {"Deneb", 0, 0}, {"Sadr", 0, 0}, {"Albireo", 0, 0},
  {"Al Fawaris", 0, 0}, {NULL, 309.341f, 33.948f}
};
static const uint8_t S_N_CROSS[] = {0, 1, 1, 2, 3, 1, 1, 4};

static const AsterismVertDef V_TEAPOT[] = {
  {"Alnasl", 0, 0}, {"Kaus Media", 0, 0}, {"Kaus Australis", 0, 0},
  {"Kaus Borealis", 0, 0}, {"Nunki", 0, 0}, {"Ascella", 0, 0},
  {"Albaldah", 0, 0}
};
static const uint8_t S_TEAPOT[] = {
  0, 1, 1, 2, 1, 3, 3, 6, 6, 4, 4, 5, 5, 2
};

static const AsterismVertDef V_W[] = {
  {"Caph", 0, 0}, {"Schedar", 0, 0}, {"Navi", 0, 0},
  {"Ruchbah", 0, 0}, {"Segin", 0, 0}
};
static const uint8_t S_W[] = {0, 1, 1, 2, 2, 3, 3, 4};

static const AsterismVertDef V_WINTER_HEX[] = {
  {"Rigel", 0, 0}, {"Aldebaran", 0, 0}, {"Capella", 0, 0},
  {"Pollux", 0, 0}, {"Procyon", 0, 0}, {"Sirius", 0, 0}
};
static const uint8_t S_WINTER_HEX[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 0};

static const AsterismVertDef V_SICKLE[] = {
  {"Ras Elased Austral", 0, 0}, {"Rasalas", 0, 0}, {"Adhafera", 0, 0},
  {"Algieba", 0, 0}, {"Regulus", 0, 0}
};
static const uint8_t S_SICKLE[] = {0, 1, 1, 2, 2, 3, 3, 4};

static const AsterismVertDef V_PLEIADES[] = {
  {"Electra", 0, 0}, {"Alcyone", 0, 0}, {"Atlas", 0, 0},
  {"Maia", 0, 0}, {"Merope", 0, 0}
};
static const uint8_t S_PLEIADES[] = {0, 1, 1, 2, 1, 3, 3, 4};

static const AsterismVertDef V_HYADES[] = {
  {"Aldebaran", 0, 0}, {"Hyadum II", 0, 0}, {"Hyadum I", 0, 0}, {"Ain", 0, 0}
};
static const uint8_t S_HYADES[] = {0, 1, 1, 2, 2, 3};

static const AsterismVertDef V_FALSE_CROSS[] = {
  {"Avior", 0, 0}, {"Aspidiske", 0, 0},
  {NULL, 140.528f, -55.011f}, {NULL, 131.176f, -54.709f}
};
static const uint8_t S_FALSE_CROSS[] = {0, 3, 3, 2, 2, 1, 1, 0};

static const AsterismVertDef V_SWORD[] = {
  {"Alnilam", 0, 0}, {NULL, 83.819f, -5.390f}, {"Nair Al Saif", 0, 0}
};
static const uint8_t S_SWORD[] = {0, 1, 1, 2};

static const AsterismVertDef V_CROWN[] = {
  {NULL, 233.233f, 31.359f}, {"Nusakan", 0, 0}, {"Alphecca", 0, 0},
  {NULL, 235.686f, 26.296f}, {NULL, 237.399f, 26.068f},
  {NULL, 239.397f, 26.878f}, {NULL, 239.713f, 29.893f}
};
static const uint8_t S_CROWN[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6};

static const AsterismVertDef V_KEYSTONE[] = {
  {NULL, 250.322f, 31.603f}, {NULL, 250.724f, 38.922f},
  {NULL, 258.762f, 36.805f}, {NULL, 255.073f, 30.926f}
};
static const uint8_t S_KEYSTONE[] = {0, 1, 1, 2, 2, 3, 3, 0};

static const AsterismVertDef V_HOOK[] = {
  {"Antares", 0, 0}, {"Alniyat", 0, 0}, {NULL, 252.968f, -38.048f},
  {NULL, 253.646f, -42.362f}, {NULL, 258.038f, -43.239f},
  {"Sargas", 0, 0}, {NULL, 266.896f, -40.127f},
  {"Girtab", 0, 0}, {"Shaula", 0, 0}, {"Lesath", 0, 0}
};
static const uint8_t S_HOOK[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9};

static const AsterismVertDef V_SPRING_TRI[] = {
  {"Arcturus", 0, 0}, {"Spica", 0, 0}, {"Denebola", 0, 0}
};
static const uint8_t S_SPRING_TRI[] = {0, 1, 1, 2, 2, 0};

static const AsterismVertDef V_CIRCLET[] = {
  {NULL, 349.292f, 3.282f}, {NULL, 351.987f, 6.379f}, {NULL, 354.992f, 5.626f},
  {NULL, 355.564f, 1.780f}, {NULL, 351.744f, 1.255f}
};
static const uint8_t S_CIRCLET[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 0};

static const AsterismVertDef V_COFFIN[] = {
  {"Sualocin", 0, 0}, {"Rotanev", 0, 0},
  {NULL, 311.665f, 16.124f}, {"Deneb Dulfim", 0, 0}
};
static const uint8_t S_COFFIN[] = {0, 1, 1, 2, 2, 3, 3, 0};

static const AsterismVertDef V_POINTERS[] = {
  {"Rigil Kentaurus", 0, 0}, {"Hadar", 0, 0}
};
static const uint8_t S_POINTERS[] = {0, 1};

static const AsterismVertDef V_KITE[] = {
  {"Nekkar", 0, 0}, {"Seginus", 0, 0}, {"Arcturus", 0, 0},
  {"Izar", 0, 0}, {"Muphrid", 0, 0}
};
static const uint8_t S_KITE[] = {0, 1, 1, 2, 2, 3, 3, 0, 2, 4};

static const AsterismVertDef V_JAR[] = {
  {"Sadachbia", 0, 0}, {NULL, 336.319f, 1.376f},
  {NULL, 337.207f, -0.020f}, {NULL, 338.839f, -0.118f}
};
static const uint8_t S_JAR[] = {1, 0, 0, 2, 0, 3};

static const AsterismVertDef V_LYRA[] = {
  {"Vega", 0, 0}, {"Sheliak", 0, 0}, {"Sulafat", 0, 0}, {NULL, 283.625f, 36.899f}
};
static const uint8_t S_LYRA[] = {0, 1, 1, 2, 2, 3, 3, 1};

static const AsterismVertDef V_DIAMOND[] = {
  {"Arcturus", 0, 0}, {"Spica", 0, 0}, {"Denebola", 0, 0}, {NULL, 194.007f, 38.318f}
};
static const uint8_t S_DIAMOND[] = {0, 1, 1, 2, 2, 3, 3, 0};

static const AsterismVertDef V_HOUSE[] = {
  {"Alfirk", 0, 0}, {"Alderamin", 0, 0}, {NULL, 332.725f, 58.203f},
  {NULL, 342.425f, 66.200f}, {"Errai", 0, 0}
};
static const uint8_t S_HOUSE[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 0};

static const AsterismVertDef V_GUARDIANS[] = {
  {"Kochab", 0, 0}, {"Pherkad", 0, 0}
};
static const uint8_t S_GUARDIANS[] = {0, 1};

static const AsterismVertDef V_KIDS[] = {
  {"Capella", 0, 0}, {"Haedus", 0, 0}, {"Hoedus II", 0, 0}
};
static const uint8_t S_KIDS[] = {0, 1, 0, 2, 1, 2};

#define DEF(title, verts, segs) \
  { title, verts, segs, (uint8_t)(sizeof(verts) / sizeof((verts)[0])), \
    (uint8_t)(sizeof(segs) / sizeof((segs)[0]) / 2) }

static const AsterismDef ASTERISMS[ASTERISM_COUNT] = {
  DEF("Big Dipper", V_BIG_DIPPER, S_BIG_DIPPER),
  DEF("Orion's Belt", V_BELT, S_BELT),
  DEF("Summer Triangle", V_SUMMER_TRI, S_SUMMER_TRI),
  DEF("Southern Cross", V_S_CROSS, S_S_CROSS),
  DEF("Winter Triangle", V_WINTER_TRI, S_WINTER_TRI),
  DEF("Little Dipper", V_LITTLE_DIPPER, S_LITTLE_DIPPER),
  DEF("Great Square", V_SQUARE, S_SQUARE),
  DEF("Northern Cross", V_N_CROSS, S_N_CROSS),
  DEF("Teapot", V_TEAPOT, S_TEAPOT),
  DEF("The W", V_W, S_W),
  DEF("Winter Hexagon", V_WINTER_HEX, S_WINTER_HEX),
  DEF("The Sickle", V_SICKLE, S_SICKLE),
  DEF("Pleiades", V_PLEIADES, S_PLEIADES),
  DEF("Hyades", V_HYADES, S_HYADES),
  DEF("False Cross", V_FALSE_CROSS, S_FALSE_CROSS),
  DEF("Orion's Sword", V_SWORD, S_SWORD),
  DEF("Northern Crown", V_CROWN, S_CROWN),
  DEF("Keystone", V_KEYSTONE, S_KEYSTONE),
  DEF("Fish Hook", V_HOOK, S_HOOK),
  DEF("Spring Triangle", V_SPRING_TRI, S_SPRING_TRI),
  DEF("Circlet", V_CIRCLET, S_CIRCLET),
  DEF("Job's Coffin", V_COFFIN, S_COFFIN),
  DEF("Southern Pointers", V_POINTERS, S_POINTERS),
  DEF("Kite", V_KITE, S_KITE),
  DEF("Water Jar", V_JAR, S_JAR),
  DEF("Lyra", V_LYRA, S_LYRA),
  DEF("Great Diamond", V_DIAMOND, S_DIAMOND),
  DEF("House of Cepheus", V_HOUSE, S_HOUSE),
  DEF("Guardians", V_GUARDIANS, S_GUARDIANS),
  DEF("The Kids", V_KIDS, S_KIDS)
};

static float s_ra[ASTERISM_MAX_VERTS];
static float s_dec[ASTERISM_MAX_VERTS];
static uint16_t s_first[ASTERISM_COUNT];
static float s_cen_ra[ASTERISM_COUNT];
static float s_cen_dec[ASTERISM_COUNT];

static void mean_eq(const float *ra, const float *dec, int n,
                    float *out_ra, float *out_dec) {
  int i;
  float minr;
  float maxr;
  float sra = 0.0f;
  float sdec = 0.0f;
  int wrap;
  if (n <= 0) {
    *out_ra = 0.0f;
    *out_dec = 0.0f;
    return;
  }
  minr = maxr = ra[0];
  for (i = 1; i < n; i++) {
    if (ra[i] < minr) {
      minr = ra[i];
    }
    if (ra[i] > maxr) {
      maxr = ra[i];
    }
  }
  wrap = (maxr - minr) > 180.0f;
  for (i = 0; i < n; i++) {
    float r = ra[i];
    if (wrap && r < 180.0f) {
      r += 360.0f;
    }
    sra += r;
    sdec += dec[i];
  }
  sra /= (float)n;
  sdec /= (float)n;
  if (sra >= 360.0f) {
    sra -= 360.0f;
  }
  *out_ra = sra;
  *out_dec = sdec;
}

void asterism_init(void) {
  int i;
  int v;
  int nverts = 0;
  for (i = 0; i < ASTERISM_COUNT; i++) {
    const AsterismDef *a = &ASTERISMS[i];
    s_first[i] = (uint16_t)nverts;
    for (v = 0; v < (int)a->n_vert && nverts < ASTERISM_MAX_VERTS; v++) {
      float ra = a->verts[v].ra_deg;
      float dec = a->verts[v].dec_deg;
      if (a->verts[v].name) {
        int star = catalog_star_by_name(a->verts[v].name);
        const PackedStar *s = catalog_star(star);
        if (s) {
          ra = catalog_ra_deg(s);
          dec = catalog_dec_deg(s);
        }
      }
      s_ra[nverts] = ra;
      s_dec[nverts] = dec;
      nverts++;
    }
    mean_eq(&s_ra[s_first[i]], &s_dec[s_first[i]], (int)a->n_vert,
            &s_cen_ra[i], &s_cen_dec[i]);
  }
}

int asterism_count(void) {
  return ASTERISM_COUNT;
}

const char *asterism_name(int index) {
  if (index < 0 || index >= ASTERISM_COUNT) {
    return NULL;
  }
  return ASTERISMS[index].title;
}

void asterism_center(int index, float *ra_deg, float *dec_deg) {
  if (index < 0 || index >= ASTERISM_COUNT) {
    *ra_deg = 0.0f;
    *dec_deg = 0.0f;
    return;
  }
  *ra_deg = s_cen_ra[index];
  *dec_deg = s_cen_dec[index];
}

int asterism_seg_count(int index) {
  if (index < 0 || index >= ASTERISM_COUNT) {
    return 0;
  }
  return (int)ASTERISMS[index].n_seg;
}

void asterism_seg_equatorial(int index, int seg,
                             float *ra0, float *dec0,
                             float *ra1, float *dec1) {
  const AsterismDef *a;
  int base;
  uint8_t ia;
  uint8_t ib;
  if (index < 0 || index >= ASTERISM_COUNT ||
      seg < 0 || seg >= (int)ASTERISMS[index].n_seg) {
    *ra0 = *dec0 = *ra1 = *dec1 = 0.0f;
    return;
  }
  a = &ASTERISMS[index];
  base = (int)s_first[index];
  ia = a->segs[seg * 2];
  ib = a->segs[seg * 2 + 1];
  *ra0 = s_ra[base + ia];
  *dec0 = s_dec[base + ia];
  *ra1 = s_ra[base + ib];
  *dec1 = s_dec[base + ib];
}
