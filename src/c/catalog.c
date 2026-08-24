#include "catalog.h"
#include "catalog_data.h"

#include <string.h>

int catalog_star_count(void) {
  return CATALOG_STAR_COUNT;
}

const PackedStar *catalog_star(int index) {
  if (index < 0 || index >= CATALOG_STAR_COUNT) {
    return NULL;
  }
  return &CATALOG_STARS[index];
}

const char *catalog_name(uint8_t name_id) {
  if (name_id == 0 || name_id > CATALOG_NAME_COUNT) {
    return NULL;
  }
  return CATALOG_NAMES[name_id];
}

float catalog_ra_deg(const PackedStar *s) {
  return ((float)s->ra) * (360.0f / 65536.0f);
}

float catalog_dec_deg(const PackedStar *s) {
  return ((float)s->dec) * 0.01f;
}

float catalog_mag(const PackedStar *s) {
  return ((float)s->mag10) * 0.1f;
}

#define CATALOG_MAX_NAMED 254
#define CATALOG_BRIGHT_MAG10 25

static uint16_t s_named[CATALOG_MAX_NAMED];
static uint16_t s_bright[CATALOG_MAX_NAMED];
static uint16_t s_faint[CATALOG_MAX_NAMED];
static int s_named_n;
static int s_bright_n;
static int s_faint_n;

static void sort_named(uint16_t *idx, int n) {
  int i;
  int j;
  for (i = 1; i < n; i++) {
    uint16_t v = idx[i];
    const char *vn = catalog_name(catalog_star(v)->name_id);
    j = i;
    while (j > 0) {
      const char *pn = catalog_name(catalog_star(idx[j - 1])->name_id);
      if (strcmp(pn, vn) <= 0) {
        break;
      }
      idx[j] = idx[j - 1];
      j--;
    }
    idx[j] = v;
  }
}

static void sort_bright(uint16_t *idx, int n) {
  int i;
  int j;
  for (i = 1; i < n; i++) {
    uint16_t v = idx[i];
    const PackedStar *vs = catalog_star(v);
    const char *vn = catalog_name(vs->name_id);
    j = i;
    while (j > 0) {
      const PackedStar *ps = catalog_star(idx[j - 1]);
      const char *pn = catalog_name(ps->name_id);
      if (ps->mag10 < vs->mag10) {
        break;
      }
      if (ps->mag10 == vs->mag10 && strcmp(pn, vn) <= 0) {
        break;
      }
      idx[j] = idx[j - 1];
      j--;
    }
    idx[j] = v;
  }
}

void catalog_build_named_index(void) {
  int i;
  s_named_n = 0;
  s_bright_n = 0;
  s_faint_n = 0;
  for (i = 0; i < catalog_star_count(); i++) {
    const PackedStar *s = catalog_star(i);
    if (s->name_id == 0) {
      continue;
    }
    if (s_named_n < CATALOG_MAX_NAMED) {
      s_named[s_named_n++] = (uint16_t)i;
    }
    if (s->mag10 <= CATALOG_BRIGHT_MAG10) {
      if (s_bright_n < CATALOG_MAX_NAMED) {
        s_bright[s_bright_n++] = (uint16_t)i;
      }
    } else if (s_faint_n < CATALOG_MAX_NAMED) {
      s_faint[s_faint_n++] = (uint16_t)i;
    }
  }
  sort_named(s_named, s_named_n);
  sort_bright(s_bright, s_bright_n);
  sort_named(s_faint, s_faint_n);
}

int catalog_named_count(void) {
  return s_named_n;
}

int catalog_named_star_index(int sorted_i) {
  if (sorted_i < 0 || sorted_i >= s_named_n) {
    return -1;
  }
  return (int)s_named[sorted_i];
}

int catalog_bright_count(void) {
  return s_bright_n;
}

int catalog_bright_star_index(int sorted_i) {
  if (sorted_i < 0 || sorted_i >= s_bright_n) {
    return -1;
  }
  return (int)s_bright[sorted_i];
}

int catalog_faint_count(void) {
  return s_faint_n;
}

int catalog_faint_star_index(int sorted_i) {
  if (sorted_i < 0 || sorted_i >= s_faint_n) {
    return -1;
  }
  return (int)s_faint[sorted_i];
}

int catalog_star_by_name(const char *name) {
  int i;
  if (!name || !name[0]) {
    return -1;
  }
  for (i = 0; i < catalog_star_count(); i++) {
    const PackedStar *s = catalog_star(i);
    const char *n = catalog_name(s->name_id);
    if (n && strcmp(n, name) == 0) {
      return i;
    }
  }
  return -1;
}
