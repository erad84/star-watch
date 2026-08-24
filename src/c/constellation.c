#include "constellation.h"
#include "constellation_data.h"

#include <stddef.h>
#include <string.h>

static const char *const FAMILY_NAMES[FAMILY_COUNT] = {
  "Ursa Major",
  "Zodiac",
  "Perseus",
  "Hercules",
  "Orion",
  "Heavenly Waters",
  "Bayer",
  "La Caille"
};

static const char *const kFamilyUrsaMajor[] = {
  "Ursa Major", "Ursa Minor", "Draco", "Canes Venatici", "Bootes",
  "Coma Berenices", "Corona Borealis", "Camelopardalis", "Lynx", "Leo Minor"
};
static const char *const kFamilyZodiac[] = {
  "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
  "Libra", "Scorpius", "Sagittarius", "Capricornus", "Aquarius", "Pisces"
};
static const char *const kFamilyPerseus[] = {
  "Cassiopeia", "Cepheus", "Andromeda", "Perseus", "Pegasus",
  "Cetus", "Auriga", "Lacerta", "Triangulum"
};
static const char *const kFamilyHercules[] = {
  "Hercules", "Sagitta", "Aquila", "Lyra", "Cygnus", "Vulpecula",
  "Hydra", "Sextans", "Crater", "Corvus", "Ophiuchus", "Serpens",
  "Scutum", "Centaurus", "Lupus", "Corona Australis", "Ara",
  "Triangulum Australe", "Crux"
};
static const char *const kFamilyOrion[] = {
  "Orion", "Canis Major", "Canis Minor", "Lepus", "Monoceros"
};
static const char *const kFamilyWaters[] = {
  "Delphinus", "Equuleus", "Eridanus", "Piscis Austrinus",
  "Carina", "Puppis", "Vela", "Pyxis", "Columba"
};
static const char *const kFamilyBayer[] = {
  "Hydrus", "Dorado", "Volans", "Apus", "Pavo", "Grus",
  "Phoenix", "Tucana", "Indus", "Chamaeleon", "Musca"
};
static const char *const kFamilyLacaille[] = {
  "Norma", "Circinus", "Telescopium", "Microscopium", "Sculptor",
  "Fornax", "Caelum", "Horologium", "Octans", "Mensa",
  "Reticulum", "Pictor", "Antlia"
};

static const char *const *const FAMILY_MEMBERS[FAMILY_COUNT] = {
  kFamilyUrsaMajor,
  kFamilyZodiac,
  kFamilyPerseus,
  kFamilyHercules,
  kFamilyOrion,
  kFamilyWaters,
  kFamilyBayer,
  kFamilyLacaille
};

static const uint8_t FAMILY_SIZES[FAMILY_COUNT] = {
  (uint8_t)(sizeof(kFamilyUrsaMajor) / sizeof(kFamilyUrsaMajor[0])),
  (uint8_t)(sizeof(kFamilyZodiac) / sizeof(kFamilyZodiac[0])),
  (uint8_t)(sizeof(kFamilyPerseus) / sizeof(kFamilyPerseus[0])),
  (uint8_t)(sizeof(kFamilyHercules) / sizeof(kFamilyHercules[0])),
  (uint8_t)(sizeof(kFamilyOrion) / sizeof(kFamilyOrion[0])),
  (uint8_t)(sizeof(kFamilyWaters) / sizeof(kFamilyWaters[0])),
  (uint8_t)(sizeof(kFamilyBayer) / sizeof(kFamilyBayer[0])),
  (uint8_t)(sizeof(kFamilyLacaille) / sizeof(kFamilyLacaille[0]))
};

static int8_t s_family_of[CONSTELLATION_COUNT];
static uint8_t s_member_idx[FAMILY_COUNT][20];
static bool s_families_ready;

static int index_by_name(const char *name) {
  int i;
  for (i = 0; i < CONSTELLATION_COUNT; i++) {
    if (strcmp(CONSTELLATIONS[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

static void ensure_families(void) {
  int f;
  int m;
  if (s_families_ready) {
    return;
  }
  for (f = 0; f < CONSTELLATION_COUNT; f++) {
    s_family_of[f] = -1;
  }
  for (f = 0; f < FAMILY_COUNT; f++) {
    for (m = 0; m < (int)FAMILY_SIZES[f]; m++) {
      int idx = index_by_name(FAMILY_MEMBERS[f][m]);
      s_member_idx[f][m] = (idx >= 0) ? (uint8_t)idx : 0;
      if (idx >= 0) {
        s_family_of[idx] = (int8_t)f;
      }
    }
  }
  s_families_ready = true;
}

static float vert_ra_deg(const ConstVertex *v) {
  return ((float)v->ra) * (360.0f / 65536.0f);
}

static float vert_dec_deg(const ConstVertex *v) {
  return ((float)v->dec) * 0.01f;
}

int constellation_count(void) {
  return CONSTELLATION_COUNT;
}

const char *constellation_name(int index) {
  if (index < 0 || index >= CONSTELLATION_COUNT) {
    return NULL;
  }
  return CONSTELLATIONS[index].name;
}

void constellation_center(int index, float *ra_deg, float *dec_deg) {
  if (index < 0 || index >= CONSTELLATION_COUNT) {
    *ra_deg = 0.0f;
    *dec_deg = 0.0f;
    return;
  }
  *ra_deg = ((float)CONSTELLATIONS[index].ra) * (360.0f / 65536.0f);
  *dec_deg = ((float)CONSTELLATIONS[index].dec) * 0.01f;
}

int constellation_seg_count(int index) {
  if (index < 0 || index >= CONSTELLATION_COUNT) {
    return 0;
  }
  return (int)CONSTELLATIONS[index].n_seg;
}

void constellation_seg_equatorial(int index, int seg,
                                  float *ra0, float *dec0,
                                  float *ra1, float *dec1) {
  const ConstellationRec *c;
  const ConstSeg *s;
  if (index < 0 || index >= CONSTELLATION_COUNT ||
      seg < 0 || seg >= (int)CONSTELLATIONS[index].n_seg) {
    *ra0 = *dec0 = *ra1 = *dec1 = 0.0f;
    return;
  }
  c = &CONSTELLATIONS[index];
  s = &CONSTELLATION_SEGS[c->first_seg + seg];
  *ra0 = vert_ra_deg(&CONSTELLATION_VERTS[s->a]);
  *dec0 = vert_dec_deg(&CONSTELLATION_VERTS[s->a]);
  *ra1 = vert_ra_deg(&CONSTELLATION_VERTS[s->b]);
  *dec1 = vert_dec_deg(&CONSTELLATION_VERTS[s->b]);
}

int constellation_family_count(void) {
  return FAMILY_COUNT;
}

const char *constellation_family_name(int family) {
  if (family < 0 || family >= FAMILY_COUNT) {
    return NULL;
  }
  return FAMILY_NAMES[family];
}

int constellation_family_member_count(int family) {
  if (family < 0 || family >= FAMILY_COUNT) {
    return 0;
  }
  return (int)FAMILY_SIZES[family];
}

int constellation_family_member(int family, int member) {
  ensure_families();
  if (family < 0 || family >= FAMILY_COUNT ||
      member < 0 || member >= (int)FAMILY_SIZES[family]) {
    return -1;
  }
  return (int)s_member_idx[family][member];
}

bool constellation_is_zodiac(int index) {
  ensure_families();
  if (index < 0 || index >= CONSTELLATION_COUNT) {
    return false;
  }
  return s_family_of[index] == FAMILY_ZODIAC;
}
