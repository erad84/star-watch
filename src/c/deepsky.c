#include "deepsky.h"

#include <stddef.h>

#define RA(h, m) ((float)(h) * 15.0f + (float)(m) * 0.25f)

typedef struct {
  const char *name;
  float ra_deg;
  float dec_deg;
  float size_deg;
} DeepSky;

static const DeepSky CLUSTERS[] = {
  {"Pleiades", RA(3, 47.0f), 24.12f, 2.0f},
  {"Hyades", RA(4, 27.0f), 15.87f, 5.5f},
  {"Beehive Cluster", RA(8, 40.4f), 19.67f, 1.5f},
  {"Double Cluster", RA(2, 20.0f), 57.13f, 1.0f},
  {"Omega Centauri", RA(13, 26.8f), -47.48f, 0.6f},
  {"47 Tucanae", RA(0, 24.1f), -72.08f, 0.5f},
  {"Hercules Cluster", RA(16, 41.7f), 36.46f, 0.3f},
  {"Jewel Box Cluster", RA(12, 53.6f), -60.37f, 0.2f},
  {"Ptolemy Cluster", RA(17, 53.9f), -34.79f, 1.3f},
  {"Wild Duck Cluster", RA(18, 51.1f), -6.27f, 0.2f},
  {"Butterfly Cluster", RA(17, 40.1f), -32.22f, 0.4f},
  {"M22", RA(18, 36.4f), -23.90f, 0.4f},
  {"M3", RA(13, 42.2f), 28.38f, 0.3f},
  {"M5", RA(15, 18.6f), 2.08f, 0.4f},
  {"M15", RA(21, 30.0f), 12.17f, 0.3f},
  {"M4", RA(16, 23.6f), -26.53f, 0.4f},
  {"M35", RA(6, 8.9f), 24.33f, 0.5f},
  {"M41", RA(6, 46.0f), -20.77f, 0.6f},
  {"M92", RA(17, 17.1f), 43.14f, 0.2f},
  {"M67", RA(8, 51.4f), 11.82f, 0.5f}
};

static const DeepSky GALAXIES[] = {
  {"Andromeda Galaxy", RA(0, 42.7f), 41.27f, 3.2f},
  {"Large Magellanic Cloud", RA(5, 23.6f), -69.75f, 10.0f},
  {"Small Magellanic Cloud", RA(0, 52.7f), -72.83f, 5.0f},
  {"Triangulum Galaxy", RA(1, 33.8f), 30.65f, 1.2f},
  {"Whirlpool Galaxy", RA(13, 29.9f), 47.20f, 0.2f},
  {"Bode's Galaxy", RA(9, 55.6f), 69.07f, 0.4f},
  {"Cigar Galaxy", RA(9, 55.8f), 69.68f, 0.2f},
  {"Centaurus A", RA(13, 25.5f), -43.02f, 0.4f},
  {"Sombrero Galaxy", RA(12, 40.0f), -11.62f, 0.15f},
  {"Pinwheel Galaxy", RA(14, 3.2f), 54.35f, 0.4f},
  {"Virgo A Galaxy", RA(12, 30.8f), 12.39f, 0.12f},
  {"Sculptor Galaxy", RA(0, 47.6f), -25.29f, 0.4f},
  {"Black Eye Galaxy", RA(12, 56.7f), 21.68f, 0.15f},
  {"Sunflower Galaxy", RA(13, 15.8f), 42.03f, 0.2f},
  {"M94", RA(12, 50.9f), 41.12f, 0.12f},
  {"M106", RA(12, 19.0f), 47.30f, 0.3f},
  {"Leo Triplet M66", RA(11, 20.2f), 12.99f, 0.15f},
  {"Leo Triplet M65", RA(11, 18.9f), 13.09f, 0.13f},
  {"M32", RA(0, 42.7f), 40.87f, 0.15f},
  {"M110", RA(0, 40.4f), 41.69f, 0.3f}
};

static const DeepSky NEBULAE[] = {
  {"Orion Nebula", RA(5, 35.4f), -5.45f, 1.5f},
  {"Carina Nebula", RA(10, 45.1f), -59.87f, 2.0f},
  {"Lagoon Nebula", RA(18, 3.8f), -24.38f, 1.5f},
  {"Eagle Nebula", RA(18, 18.8f), -13.78f, 0.5f},
  {"Ring Nebula", RA(18, 53.6f), 33.03f, 0.02f},
  {"Dumbbell Nebula", RA(19, 59.6f), 22.72f, 0.13f},
  {"Crab Nebula", RA(5, 34.5f), 22.02f, 0.1f},
  {"Helix Nebula", RA(22, 29.6f), -20.84f, 0.3f},
  {"Tarantula Nebula", RA(5, 38.6f), -69.10f, 0.7f},
  {"Veil Nebula", RA(20, 56.4f), 31.72f, 3.0f},
  {"North America Nebula", RA(20, 59.3f), 44.53f, 2.0f},
  {"Trifid Nebula", RA(18, 2.6f), -23.03f, 0.4f},
  {"Omega Nebula", RA(18, 20.8f), -16.18f, 0.4f},
  {"Rosette Nebula", RA(6, 33.7f), 5.00f, 1.3f},
  {"California Nebula", RA(4, 3.3f), 36.42f, 2.5f},
  {"M78", RA(5, 46.7f), 0.05f, 0.13f},
  {"Little Dumbbell Nebula", RA(1, 42.3f), 51.57f, 0.03f},
  {"Saturn Nebula", RA(21, 4.2f), -11.37f, 0.01f},
  {"Cat's Eye Nebula", RA(17, 58.6f), 66.63f, 0.01f},
  {"Cocoon Nebula", RA(21, 53.4f), 47.27f, 0.2f}
};

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void object_at(const DeepSky *list, int n, int index,
                      const char **name, float *ra, float *dec, float *size) {
  if (index < 0 || index >= n) {
    if (name) {
      *name = NULL;
    }
    if (ra) {
      *ra = 0.0f;
    }
    if (dec) {
      *dec = 0.0f;
    }
    if (size) {
      *size = 0.0f;
    }
    return;
  }
  if (name) {
    *name = list[index].name;
  }
  if (ra) {
    *ra = list[index].ra_deg;
  }
  if (dec) {
    *dec = list[index].dec_deg;
  }
  if (size) {
    *size = list[index].size_deg;
  }
}

int cluster_count(void) {
  return COUNT_OF(CLUSTERS);
}

const char *cluster_name(int index) {
  const char *name = NULL;
  object_at(CLUSTERS, COUNT_OF(CLUSTERS), index, &name, NULL, NULL, NULL);
  return name;
}

void cluster_equatorial(int index, float *ra_deg, float *dec_deg) {
  object_at(CLUSTERS, COUNT_OF(CLUSTERS), index, NULL, ra_deg, dec_deg, NULL);
}

float cluster_size_deg(int index) {
  float size = 0.0f;
  object_at(CLUSTERS, COUNT_OF(CLUSTERS), index, NULL, NULL, NULL, &size);
  return size;
}

int galaxy_count(void) {
  return COUNT_OF(GALAXIES);
}

const char *galaxy_name(int index) {
  const char *name = NULL;
  object_at(GALAXIES, COUNT_OF(GALAXIES), index, &name, NULL, NULL, NULL);
  return name;
}

void galaxy_equatorial(int index, float *ra_deg, float *dec_deg) {
  object_at(GALAXIES, COUNT_OF(GALAXIES), index, NULL, ra_deg, dec_deg, NULL);
}

float galaxy_size_deg(int index) {
  float size = 0.0f;
  object_at(GALAXIES, COUNT_OF(GALAXIES), index, NULL, NULL, NULL, &size);
  return size;
}

int nebula_count(void) {
  return COUNT_OF(NEBULAE);
}

const char *nebula_name(int index) {
  const char *name = NULL;
  object_at(NEBULAE, COUNT_OF(NEBULAE), index, &name, NULL, NULL, NULL);
  return name;
}

void nebula_equatorial(int index, float *ra_deg, float *dec_deg) {
  object_at(NEBULAE, COUNT_OF(NEBULAE), index, NULL, ra_deg, dec_deg, NULL);
}

float nebula_size_deg(int index) {
  float size = 0.0f;
  object_at(NEBULAE, COUNT_OF(NEBULAE), index, NULL, NULL, NULL, &size);
  return size;
}
