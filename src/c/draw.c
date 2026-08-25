#include "draw.h"
#include "asterism.h"
#include "catalog.h"
#include "constellation.h"
#include "deepsky.h"
#include "iss.h"
#include "lagrange.h"
#include "sky.h"
#include "starwatch.h"

#include <stdio.h>

static int s_screen_w = PBL_DISPLAY_WIDTH;

#define FAINT_MAG10 25
#ifdef PBL_PLATFORM_FLINT
#define LARGE_R 4
#define CROSSHAIR 6
#define NAME_BAR_H 22
#else
#define LARGE_R 5
#define CROSSHAIR PBL_IF_ROUND_ELSE(10, 8)
#define NAME_BAR_H PBL_IF_ROUND_ELSE(28, 26)
#endif

#ifdef PBL_ROUND
#define NAME_FONT FONT_KEY_GOTHIC_18_BOLD
#define HEAD_FONT FONT_KEY_GOTHIC_18_BOLD
#define DIR_FONT FONT_KEY_GOTHIC_14_BOLD
#else
#ifdef PBL_PLATFORM_FLINT
#define NAME_FONT FONT_KEY_GOTHIC_14_BOLD
#define HEAD_FONT FONT_KEY_GOTHIC_14_BOLD
#define DIR_FONT FONT_KEY_GOTHIC_09
#else
#define NAME_FONT FONT_KEY_GOTHIC_18_BOLD
#define HEAD_FONT FONT_KEY_GOTHIC_18_BOLD
#define DIR_FONT FONT_KEY_GOTHIC_14_BOLD
#endif
#endif

#define UI_DIM PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite)
#define UI_MID PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)

static Layer *s_mq_layer;
static const char *s_mq_text;
static int s_mq_scroll;
static int s_mq_max;
static int s_mq_dir = 1;
static int s_mq_pause;
static AppTimer *s_mq_timer;

static GColor planet_color(int id) {
  switch (id) {
    case BODY_SUN:
      return GColorYellow;
    case BODY_MOON:
      return GColorLightGray;
    case BODY_MERCURY:
      return GColorLightGray;
    case BODY_VENUS:
      return GColorIcterine;
    case BODY_MARS:
      return GColorRed;
    case BODY_JUPITER:
      return GColorOrange;
    case BODY_SATURN:
      return GColorPastelYellow;
    case BODY_URANUS:
      return GColorCyan;
    case BODY_NEPTUNE:
      return GColorBlue;
    default:
      return GColorWhite;
  }
}

static int planet_radius(int id) {
  int r;
  switch (id) {
    case BODY_MOON:
      r = 14;
      break;
    case BODY_SUN:
      r = 12;
      break;
    case BODY_JUPITER:
      r = 5;
      break;
    case BODY_SATURN:
    case BODY_VENUS:
      r = 4;
      break;
    default:
      r = 3;
      break;
  }
#ifdef PBL_PLATFORM_FLINT
  r = (r * 3) / 4;
  if (r < 2) {
    r = 2;
  }
#endif
  return r;
}

static int ang_to_r(float size_deg) {
  return sky_ang_radius_px(size_deg);
}

static int leader_r(int r) {
  return (r > LARGE_R) ? 0 : r;
}

static void draw_body_mark(GContext *ctx, GPoint p, int r, GColor color, bool ring) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  if (ring && r > LARGE_R) {
    graphics_draw_circle(ctx, p, r);
    graphics_fill_circle(ctx, p, 1);
  } else {
    graphics_fill_circle(ctx, p, r);
  }
}

static int star_radius(float mag) {
#ifdef PBL_PLATFORM_FLINT
  if (mag <= 0.0f) {
    return 2;
  }
  if (mag <= 1.5f) {
    return 2;
  }
  return 1;
#else
  if (mag <= 0.0f) {
    return 3;
  }
  if (mag <= 1.5f) {
    return 2;
  }
  return 1;
#endif
}

static int ui_hpad(int y, int w, int h) {
#ifdef PBL_ROUND
  int r = ((w < h ? w : h) / 2) - 8;
  int dy = y - h / 2;
  int inner = r * r - dy * dy;
  int g;
  int left;
  if (inner <= 0) {
    return w / 2 - 8;
  }
  g = r;
  g = (g + inner / g) / 2;
  g = (g + inner / g) / 2;
  g = (g + inner / g) / 2;
  g = (g + inner / g) / 2;
  left = w / 2 - g;
  if (left < 10) {
    left = 10;
  }
  return left;
#else
  (void)y;
  (void)h;
  return (w <= 144) ? 2 : 2;
#endif
}

static int icon_x(GRect bounds) {
#ifdef PBL_ROUND
  return bounds.size.w - 28;
#elif defined(PBL_PLATFORM_FLINT)
  return bounds.size.w - 8;
#else
  return bounds.size.w - 10;
#endif
}

static int light_icon_y(GRect bounds) {
#ifdef PBL_ROUND
  return bounds.size.h / 2 - 58;
#elif defined(PBL_PLATFORM_FLINT)
  return 10;
#else
  return 22;
#endif
}

static int sky_icon_y(GRect bounds) {
#ifdef PBL_ROUND
  return bounds.size.h / 2 + 58;
#elif defined(PBL_PLATFORM_FLINT)
  return bounds.size.h - 34;
#else
  return bounds.size.h - 48;
#endif
}

static void marquee_tick(void *context) {
  (void)context;
  s_mq_timer = NULL;
  if (s_mq_max <= 0) {
    return;
  }
  s_mq_timer = app_timer_register(80, marquee_tick, NULL);
  if (s_mq_pause > 0) {
    s_mq_pause--;
  } else {
    s_mq_scroll += s_mq_dir * 2;
    if (s_mq_scroll <= 0) {
      s_mq_scroll = 0;
      s_mq_dir = 1;
      s_mq_pause = 10;
    } else if (s_mq_scroll >= s_mq_max) {
      s_mq_scroll = s_mq_max;
      s_mq_dir = -1;
      s_mq_pause = 10;
    }
  }
  if (s_mq_layer) {
    layer_mark_dirty(s_mq_layer);
  }
}

static void marquee_kick(void) {
  if (!s_mq_timer && s_mq_max > 0) {
    s_mq_timer = app_timer_register(80, marquee_tick, NULL);
  }
}

static void draw_marquee_text(GContext *ctx, GRect clip, const char *text, GFont font,
                             GColor color, GTextAlignment fallback_align) {
  GSize sz;
  if (!text) {
    text = "";
  }
  if (text != s_mq_text) {
    s_mq_text = text;
    s_mq_scroll = 0;
    s_mq_dir = 1;
    s_mq_pause = 8;
  }
  sz = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 1000, clip.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  graphics_context_set_text_color(ctx, color);
  if (sz.w <= clip.size.w) {
    s_mq_max = 0;
    graphics_draw_text(ctx, text, font, clip,
                       GTextOverflowModeTrailingEllipsis, fallback_align, NULL);
    return;
  }
  s_mq_max = sz.w - clip.size.w;
  if (s_mq_scroll > s_mq_max) {
    s_mq_scroll = s_mq_max;
  }
  graphics_draw_text(ctx, text, font,
                     GRect(clip.origin.x - s_mq_scroll, clip.origin.y,
                           sz.w + 8, clip.size.h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  marquee_kick();
}

static void draw_crosshair(GContext *ctx, GPoint c) {
  graphics_context_set_stroke_color(ctx, UI_DIM);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(c.x - CROSSHAIR, c.y), GPoint(c.x + CROSSHAIR, c.y));
  graphics_draw_line(ctx, GPoint(c.x, c.y - CROSSHAIR), GPoint(c.x, c.y + CROSSHAIR));
  graphics_draw_circle(ctx, c, PBL_IF_ROUND_ELSE(6, 5));
}

static void draw_heading(GContext *ctx, GRect bounds) {
  char buf[8];
  int deg = (int)(g_app.look_az_deg + 0.5f);
  int pad;
  while (deg < 0) {
    deg += 360;
  }
  while (deg >= 360) {
    deg -= 360;
  }
  snprintf(buf, sizeof(buf), "%d\xC2\xB0", deg);
  pad = ui_hpad(8, bounds.size.w, bounds.size.h);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf,
                     fonts_get_system_font(HEAD_FONT),
                     GRect(pad, PBL_IF_ROUND_ELSE(8, -2),
                           bounds.size.w - 2 * pad, 24),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_overlay(GContext *ctx, GRect bounds, const char *text) {
  int top = g_app.show_heading ? PBL_IF_ROUND_ELSE(28, 20) : PBL_IF_ROUND_ELSE(16, 2);
#ifdef PBL_PLATFORM_FLINT
  if (g_app.show_heading) {
    top = 16;
  } else {
    top = 1;
  }
#endif
  {
    int pad = ui_hpad(top + 10, bounds.size.w, bounds.size.h);
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, text,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(pad, top, bounds.size.w - 2 * pad, 36),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

static void draw_name_bar(GContext *ctx, GRect bounds, const char *name) {
  int y = bounds.size.h - NAME_BAR_H;
  int pad = ui_hpad(y + NAME_BAR_H / 2, bounds.size.w, bounds.size.h);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, NAME_BAR_H), 0, GCornerNone);
  draw_marquee_text(ctx,
                    GRect(pad, y - 2, bounds.size.w - 2 * pad, NAME_BAR_H),
                    name, fonts_get_system_font(NAME_FONT), GColorWhite,
                    GTextAlignmentCenter);
}

static void draw_leader(GContext *ctx, GPoint center, GPoint best_pt, int best_r) {
  int dx = best_pt.x - center.x;
  int dy = best_pt.y - center.y;
  int dist = dx * dx + dy * dy;
  GPoint end = best_pt;
  graphics_context_set_stroke_color(ctx, GColorIcterine);
  graphics_context_set_stroke_width(ctx, 1);
  if (dist > 1 && best_r > 0) {
    int mag = 1;
    while (mag * mag < dist) {
      mag++;
    }
    end.x = best_pt.x - (dx * best_r) / mag;
    end.y = best_pt.y - (dy * best_r) / mag;
  }
  graphics_draw_line(ctx, center, end);
  if (best_r <= 0) {
    graphics_context_set_fill_color(ctx, GColorIcterine);
    graphics_fill_circle(ctx, end, 3);
  }
}

static bool want_kind(uint8_t kind, bool show) {
  return show || (g_app.target_mode == TARGET_NAMED && g_app.target_kind == kind);
}

static bool named_eq(float ra, float dec, bool valid, int radius, const char *label,
                     int16_t *px, int16_t *py, int *out_r, const char **name,
                     bool *on_screen) {
  *name = label;
  *out_r = radius;
  if (!valid) {
    *on_screen = false;
    *px = 0;
    *py = 0;
    return true;
  }
  sky_aim_equatorial(ra, dec, px, py, on_screen);
  return true;
}

static bool named_target(int16_t *px, int16_t *py, int *radius, const char **name, bool *on_screen) {
  if (g_app.target_kind == TARGET_KIND_BODY) {
    int id = (int)g_app.target_index;
    if (id < 0 || id >= PLANET_COUNT) {
      return false;
    }
    return named_eq(g_app.planet_ra_deg[id], g_app.planet_dec_deg[id],
                    g_app.planets_valid, planet_radius(id), BODY_NAMES[id],
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_DWARF) {
    int id = (int)g_app.target_index;
    if (id < 0 || id >= DWARF_COUNT) {
      return false;
    }
    return named_eq(g_app.dwarf_ra_deg[id], g_app.dwarf_dec_deg[id],
                    g_app.dwarfs_valid, 3, DWARF_NAMES[id],
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_ASTEROID) {
    int id = (int)g_app.target_index;
    if (id < 0 || id >= ASTEROID_COUNT) {
      return false;
    }
    return named_eq(g_app.asteroid_ra_deg[id], g_app.asteroid_dec_deg[id],
                    g_app.asteroids_valid, 2, ASTEROID_NAMES[id],
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_SAT) {
    int id = (int)g_app.target_index;
    float az;
    float alt;
    if (id < 0 || id >= SAT_COUNT) {
      return false;
    }
    *name = sat_name(id);
    *radius = 4;
    if (iss_ready_index(id) && iss_horiz(&az, &alt)) {
      sky_aim_from_horiz(az, alt, px, py, on_screen);
      return true;
    }
    return named_eq(g_app.sat_ra_deg[id], g_app.sat_dec_deg[id],
                    sat_has_pos(id), 4, sat_name(id),
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_LAGRANGE) {
    float ra;
    float dec;
    int id = (int)g_app.target_index;
    if (id < 0 || id >= lagrange_count()) {
      return false;
    }
    lagrange_equatorial(id, &ra, &dec);
    return named_eq(ra, dec, lagrange_ready(),
                    leader_r(ang_to_r(lagrange_size_deg(id))),
                    lagrange_name(id),
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_CLUSTER) {
    float ra;
    float dec;
    cluster_equatorial((int)g_app.target_index, &ra, &dec);
    return named_eq(ra, dec, true,
                    leader_r(ang_to_r(cluster_size_deg((int)g_app.target_index))),
                    cluster_name((int)g_app.target_index) ?
                    cluster_name((int)g_app.target_index) : "Cluster",
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_GALAXY) {
    float ra;
    float dec;
    galaxy_equatorial((int)g_app.target_index, &ra, &dec);
    return named_eq(ra, dec, true,
                    leader_r(ang_to_r(galaxy_size_deg((int)g_app.target_index))),
                    galaxy_name((int)g_app.target_index) ?
                    galaxy_name((int)g_app.target_index) : "Galaxy",
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_NEBULA) {
    float ra;
    float dec;
    nebula_equatorial((int)g_app.target_index, &ra, &dec);
    return named_eq(ra, dec, true,
                    leader_r(ang_to_r(nebula_size_deg((int)g_app.target_index))),
                    nebula_name((int)g_app.target_index) ?
                    nebula_name((int)g_app.target_index) : "Nebula",
                    px, py, radius, name, on_screen);
  }
  if (g_app.target_kind == TARGET_KIND_CONSTELL) {
    float ra;
    float dec;
    *radius = 0;
    *name = constellation_name((int)g_app.target_index);
    if (!*name) {
      *name = "Constellation";
    }
    constellation_center((int)g_app.target_index, &ra, &dec);
    sky_aim_equatorial(ra, dec, px, py, on_screen);
    return true;
  }
  if (g_app.target_kind == TARGET_KIND_ASTERISM) {
    float ra;
    float dec;
    *radius = 0;
    *name = asterism_name((int)g_app.target_index);
    if (!*name) {
      *name = "Asterism";
    }
    asterism_center((int)g_app.target_index, &ra, &dec);
    sky_aim_equatorial(ra, dec, px, py, on_screen);
    return true;
  }
  {
    int idx = (int)g_app.target_index;
    const PackedStar *s = catalog_star(idx);
    if (!s) {
      return false;
    }
    *name = catalog_name(s->name_id);
    if (!*name) {
      *name = "Star";
    }
    *radius = star_radius(catalog_mag(s));
    sky_aim_equatorial(catalog_ra_deg(s), catalog_dec_deg(s), px, py, on_screen);
    return true;
  }
}

static void draw_one_constellation(GContext *ctx, int index, GColor color) {
  int n = constellation_seg_count(index);
  int s;
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  for (s = 0; s < n; s++) {
    float ra0, dec0, ra1, dec1;
    int16_t x0, y0, x1, y1;
    constellation_seg_equatorial(index, s, &ra0, &dec0, &ra1, &dec1);
    if (!sky_project_equatorial(ra0, dec0, &x0, &y0) ||
        !sky_project_equatorial(ra1, dec1, &x1, &y1)) {
      continue;
    }
    graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x1, y1));
  }
}

static void draw_one_asterism(GContext *ctx, int index, GColor color) {
  int n = asterism_seg_count(index);
  int s;
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  for (s = 0; s < n; s++) {
    float ra0, dec0, ra1, dec1;
    int16_t x0, y0, x1, y1;
    asterism_seg_equatorial(index, s, &ra0, &dec0, &ra1, &dec1);
    if (!sky_project_equatorial(ra0, dec0, &x0, &y0) ||
        !sky_project_equatorial(ra1, dec1, &x1, &y1)) {
      continue;
    }
    graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x1, y1));
  }
}

static void draw_constellations(GContext *ctx) {
  int highlight = -1;
  int ast_hl = -1;
  int i;
  int n = constellation_count();
  if (g_app.target_mode == TARGET_NAMED &&
      g_app.target_kind == TARGET_KIND_CONSTELL) {
    highlight = (int)g_app.target_index;
  }
  if (g_app.target_mode == TARGET_NAMED &&
      g_app.target_kind == TARGET_KIND_ASTERISM) {
    ast_hl = (int)g_app.target_index;
  }
  if (g_app.sky_mode == SKY_ZODIAC || g_app.sky_mode == SKY_CONSTELL ||
      (g_app.sky_mode == SKY_STARS && g_app.show_constellations)) {
    for (i = 0; i < n; i++) {
      if (i == highlight) {
        continue;
      }
      if (g_app.sky_mode == SKY_ZODIAC && !constellation_is_zodiac(i)) {
        continue;
      }
      draw_one_constellation(ctx, i, UI_DIM);
    }
  }
  if (g_app.show_asterisms && g_app.sky_mode == SKY_STARS) {
    int na = asterism_count();
    for (i = 0; i < na; i++) {
      if (i == ast_hl) {
        continue;
      }
      draw_one_asterism(ctx, i, UI_DIM);
    }
  }
  if (highlight >= 0) {
    draw_one_constellation(ctx, highlight, GColorIcterine);
  }
  if (ast_hl >= 0) {
    draw_one_asterism(ctx, ast_hl, GColorIcterine);
  }
}

static bool nearest_constellation(GPoint center, GPoint *best_pt, const char **best_name) {
  int best_d2 = 0x7fffffff;
  int i;
  int n = constellation_count();
  *best_name = NULL;
  for (i = 0; i < n; i++) {
    float ra;
    float dec;
    int16_t px;
    int16_t py;
    bool on_screen = false;
    int dx;
    int dy;
    int d2;
    if (g_app.sky_mode == SKY_ZODIAC && !constellation_is_zodiac(i)) {
      continue;
    }
    constellation_center(i, &ra, &dec);
    sky_aim_equatorial(ra, dec, &px, &py, &on_screen);
    if (!on_screen) {
      continue;
    }
    dx = px - center.x;
    dy = py - center.y;
    d2 = dx * dx + dy * dy;
    if (d2 < best_d2) {
      best_d2 = d2;
      *best_pt = GPoint(px, py);
      *best_name = constellation_name(i);
    }
  }
  return *best_name != NULL;
}

static const char *const HORIZON_DIRS[16] = {
  "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
  "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
};

static void draw_sky_ring(GContext *ctx, bool (*point)(int, int16_t *, int16_t *)) {
  int16_t prev_x = 0;
  int16_t prev_y = 0;
  bool has_prev = false;
  int i;
  for (i = 0; i <= SKY_HORIZON_STEPS; i++) {
    int16_t px;
    int16_t py;
    if (!point(i, &px, &py)) {
      has_prev = false;
      continue;
    }
    if (has_prev) {
      graphics_draw_line(ctx, GPoint(prev_x, prev_y), GPoint(px, py));
    }
    prev_x = px;
    prev_y = py;
    has_prev = true;
  }
}

static void draw_horizon(GContext *ctx) {
  int k;
  graphics_context_set_stroke_color(ctx, UI_MID);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_text_color(ctx, GColorWhite);
  draw_sky_ring(ctx, sky_horizon_point);
  if (!g_app.show_cardinals) {
    return;
  }
  for (k = 0; k < 16; k++) {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    float az = (float)k * 22.5f;
    float tip = (k % 4 == 0) ? 6.0f : ((k % 2 == 0) ? 4.5f : 3.0f);
    if (!sky_project_horiz(az, 0.0f, &x0, &y0) ||
        !sky_project_horiz(az, tip, &x1, &y1)) {
      continue;
    }
    graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x1, y1));
    graphics_draw_text(ctx, HORIZON_DIRS[k], fonts_get_system_font(DIR_FONT),
#ifdef PBL_PLATFORM_FLINT
                       GRect(x1 - 14, y1 - 12, 28, 14),
#else
                       GRect(x1 - 20, y1 - 16, 40, 18),
#endif
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void draw_lightbulb(GContext *ctx, GRect bounds) {
  const int x = icon_x(bounds);
  const int y = light_icon_y(bounds);
  GColor fill;
  GColor stroke;
  switch (g_app.light_mode) {
    case LIGHT_WHITE:
      fill = PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite);
      stroke = GColorWhite;
      break;
#ifdef PBL_PLATFORM_EMERY
    case LIGHT_RED:
      fill = GColorRed;
      stroke = GColorWhite;
      break;
#endif
    default:
      fill = GColorBlack;
      stroke = UI_DIM;
      break;
  }
  graphics_context_set_fill_color(ctx, fill);
  graphics_context_set_stroke_color(ctx, stroke);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_fill_circle(ctx, GPoint(x, y + 5), 5);
  graphics_draw_circle(ctx, GPoint(x, y + 5), 5);
  graphics_fill_rect(ctx, GRect(x - 2, y + 9, 5, 3), 0, GCornerNone);
  graphics_draw_rect(ctx, GRect(x - 3, y + 12, 7, 3));
  graphics_draw_line(ctx, GPoint(x - 2, y + 13), GPoint(x + 2, y + 13));
  if (g_app.light_mode != LIGHT_OFF) {
    graphics_draw_line(ctx, GPoint(x - 9, y + 5), GPoint(x - 7, y + 5));
    graphics_draw_line(ctx, GPoint(x - 8, y + 1), GPoint(x - 6, y + 3));
    graphics_draw_line(ctx, GPoint(x - 8, y + 9), GPoint(x - 6, y + 7));
  }
}

static void draw_menu_arrow(GContext *ctx, GRect bounds) {
  const int x = icon_x(bounds);
  const int y = bounds.size.h / 2;
  graphics_context_set_stroke_color(ctx, UI_MID);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(x - 5, y - 6), GPoint(x + 3, y));
  graphics_draw_line(ctx, GPoint(x + 3, y), GPoint(x - 5, y + 6));
  graphics_draw_line(ctx, GPoint(x - 5, y - 6), GPoint(x - 5, y + 6));
}

static void draw_constellation_icon(GContext *ctx, GRect bounds) {
  const int x = icon_x(bounds);
  const int y = sky_icon_y(bounds);
  GPoint a = GPoint(x - 6, y + 6);
  GPoint b = GPoint(x + 5, y + 3);
  GPoint c = GPoint(x, y - 6);
  graphics_context_set_stroke_width(ctx, 1);
  if (g_app.target_mode == TARGET_NAMED) {
    graphics_context_set_stroke_color(ctx, GColorIcterine);
    graphics_draw_line(ctx, GPoint(x - 5, y), GPoint(x + 5, y));
    graphics_draw_line(ctx, GPoint(x, y - 5), GPoint(x, y + 5));
    graphics_draw_circle(ctx, GPoint(x, y), 8);
    return;
  }
  if (g_app.sky_mode == SKY_ZODIAC) {
    GPoint z0 = GPoint(x - 5, y - 6);
    GPoint z1 = GPoint(x + 5, y - 6);
    GPoint z2 = GPoint(x - 5, y + 6);
    GPoint z3 = GPoint(x + 5, y + 6);
    graphics_context_set_stroke_color(ctx, GColorIcterine);
    graphics_context_set_fill_color(ctx, GColorIcterine);
    graphics_draw_line(ctx, z0, z1);
    graphics_draw_line(ctx, z1, z2);
    graphics_draw_line(ctx, z2, z3);
    graphics_fill_circle(ctx, z0, 2);
    graphics_fill_circle(ctx, z1, 2);
    graphics_fill_circle(ctx, z2, 2);
    graphics_fill_circle(ctx, z3, 2);
    return;
  }
  if (g_app.sky_mode == SKY_CONSTELL) {
    graphics_context_set_stroke_color(ctx, GColorIcterine);
    graphics_context_set_fill_color(ctx, GColorIcterine);
    graphics_draw_line(ctx, a, b);
    graphics_draw_line(ctx, b, c);
    graphics_draw_line(ctx, c, a);
    graphics_fill_circle(ctx, a, 2);
    graphics_fill_circle(ctx, b, 2);
    graphics_fill_circle(ctx, c, 2);
    return;
  }
  (void)a;
  (void)b;
  (void)c;
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(x - 6, y), GPoint(x + 6, y));
  graphics_draw_line(ctx, GPoint(x, y - 6), GPoint(x, y + 6));
}

static void note_nearest(GPoint center, GPoint pt, int r, const char *name,
                         int *best_d2, GPoint *best_pt, int *best_r,
                         const char **best_name) {
  int dx;
  int dy;
  int d2;
  if (!name) {
    return;
  }
  dx = pt.x - center.x;
  dy = pt.y - center.y;
  d2 = dx * dx + dy * dy;
  if (d2 < *best_d2) {
    *best_d2 = d2;
    *best_pt = pt;
    *best_name = name;
    *best_r = r;
  }
}

static bool plot_eq(float ra, float dec, int16_t *px, int16_t *py) {
  if (!g_app.show_below_horizon && sky_alt_equatorial(ra, dec) < 0.0f) {
    return false;
  }
  return sky_project_equatorial(ra, dec, px, py);
}

static void draw_iss_mark(GContext *ctx, GPoint p) {
#ifdef PBL_PLATFORM_FLINT
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(p.x - 2, p.y - 1, 5, 3), 0, GCornerNone);
  graphics_draw_line(ctx, GPoint(p.x - 4, p.y), GPoint(p.x + 4, p.y));
#else
  graphics_context_set_fill_color(ctx, GColorCyan);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(p.x - 3, p.y - 2, 7, 5), 0, GCornerNone);
  graphics_draw_line(ctx, GPoint(p.x - 6, p.y), GPoint(p.x + 6, p.y));
#endif
}

static void draw_dso_list(GContext *ctx, GRect bounds, GPoint center,
                          int count,
                          void (*eq)(int, float *, float *),
                          const char *(*name_at)(int),
                          float (*size_at)(int),
                          GColor color,
                          int *best_d2, GPoint *best_pt, int *best_r,
                          const char **best_name) {
  int i;
  for (i = 0; i < count; i++) {
    float ra;
    float dec;
    int16_t px;
    int16_t py;
    int r;
    eq(i, &ra, &dec);
    if (!plot_eq(ra, dec, &px, &py)) {
      continue;
    }
    r = ang_to_r(size_at(i));
    draw_body_mark(ctx, GPoint(px, py), r, color, true);
    if (px < 0 || py < 0 || px >= bounds.size.w || py >= bounds.size.h) {
      continue;
    }
    note_nearest(center, GPoint(px, py), leader_r(r), name_at(i),
                 best_d2, best_pt, best_r, best_name);
  }
}

static void draw_movers(GContext *ctx, GRect bounds, GPoint center,
                        const float *ra, const float *dec, const char *const *names,
                        int count, bool valid, GColor color, int radius,
                        int *best_d2, GPoint *best_pt, int *best_r,
                        const char **best_name) {
  int i;
  if (!valid) {
    return;
  }
  for (i = 0; i < count; i++) {
    int16_t px;
    int16_t py;
    if (!plot_eq(ra[i], dec[i], &px, &py)) {
      continue;
    }
    graphics_context_set_fill_color(ctx, color);
    graphics_fill_circle(ctx, GPoint(px, py), radius);
    if (px < 0 || py < 0 || px >= bounds.size.w || py >= bounds.size.h) {
      continue;
    }
    note_nearest(center, GPoint(px, py), radius, names[i],
                 best_d2, best_pt, best_r, best_name);
  }
}

void draw_sky_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);

  s_mq_layer = layer;
  s_screen_w = bounds.size.w;
  iss_update();

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  sky_set_look(g_app.look_az_deg, g_app.look_alt_deg);

  int best_d2 = 0x7fffffff;
  GPoint best_pt = center;
  const char *best_name = NULL;
  int best_r = 0;

  int nstars = catalog_star_count();
  if (g_app.sky_mode == SKY_STARS) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    for (int i = 0; i < nstars; i++) {
      int16_t px;
      int16_t py;
      const PackedStar *s = catalog_star(i);
      int8_t mag10;
      int16_t alt;
      int r;
      if (!s) {
        continue;
      }
      mag10 = s->mag10;
      if (mag10 > FAINT_MAG10) {
        if (!g_app.show_faint_stars) {
          continue;
        }
      } else if (!g_app.show_bright_stars) {
        continue;
      }
      alt = sky_star_alt_deg(i);
      if (!g_app.show_below_horizon && alt < 0) {
        continue;
      }
      if (!sky_star_on_screen(i, &px, &py)) {
        continue;
      }
      if (mag10 <= 0) {
        r = 3;
      } else if (mag10 <= 15) {
        r = 2;
      } else {
        r = 1;
      }
      graphics_context_set_fill_color(ctx, alt < 0 ? UI_DIM : GColorWhite);
      if (r <= 1) {
        graphics_fill_rect(ctx, GRect(px, py, 2, 2), 0, GCornerNone);
      } else {
        graphics_fill_circle(ctx, GPoint(px, py), r);
      }
      if (px < 0 || py < 0 || px >= bounds.size.w || py >= bounds.size.h) {
        continue;
      }
      note_nearest(center, GPoint(px, py), r, catalog_name(s->name_id),
                   &best_d2, &best_pt, &best_r, &best_name);
    }
  }

  if (g_app.planets_valid && g_app.sky_mode == SKY_STARS) {
    for (int i = 0; i < PLANET_COUNT; i++) {
      int16_t px;
      int16_t py;
      bool show;
      int r;
      if (i == BODY_SUN) {
        show = g_app.show_sun;
      } else if (i == BODY_MOON) {
        show = g_app.show_moon;
      } else {
        show = g_app.show_planets;
      }
      if (g_app.target_mode == TARGET_NAMED &&
          g_app.target_kind == TARGET_KIND_BODY &&
          (int)g_app.target_index == i) {
        show = true;
      }
      if (!show) {
        continue;
      }
      if (!plot_eq(g_app.planet_ra_deg[i], g_app.planet_dec_deg[i], &px, &py)) {
        continue;
      }
      r = planet_radius(i);
      draw_body_mark(ctx, GPoint(px, py), r, planet_color(i), false);
      if (px < 0 || py < 0 || px >= bounds.size.w || py >= bounds.size.h) {
        continue;
      }
      note_nearest(center, GPoint(px, py), r, BODY_NAMES[i],
                   &best_d2, &best_pt, &best_r, &best_name);
    }
  }

  if (g_app.sky_mode == SKY_STARS) {
    if (want_kind(TARGET_KIND_DWARF, g_app.show_dwarfs)) {
      draw_movers(ctx, bounds, center, g_app.dwarf_ra_deg, g_app.dwarf_dec_deg,
                  DWARF_NAMES, DWARF_COUNT, g_app.dwarfs_valid, GColorLightGray, 2,
                  &best_d2, &best_pt, &best_r, &best_name);
    }
    if (want_kind(TARGET_KIND_ASTEROID, g_app.show_asteroids)) {
      draw_movers(ctx, bounds, center, g_app.asteroid_ra_deg, g_app.asteroid_dec_deg,
                  ASTEROID_NAMES, ASTEROID_COUNT, g_app.asteroids_valid,
                  UI_DIM, 2, &best_d2, &best_pt, &best_r, &best_name);
    }
    if (want_kind(TARGET_KIND_CLUSTER, g_app.show_clusters)) {
      draw_dso_list(ctx, bounds, center, cluster_count(), cluster_equatorial,
                    cluster_name, cluster_size_deg, GColorIcterine,
                    &best_d2, &best_pt, &best_r, &best_name);
    }
    if (want_kind(TARGET_KIND_GALAXY, g_app.show_galaxies)) {
      draw_dso_list(ctx, bounds, center, galaxy_count(), galaxy_equatorial,
                    galaxy_name, galaxy_size_deg, GColorPurple,
                    &best_d2, &best_pt, &best_r, &best_name);
    }
    if (want_kind(TARGET_KIND_NEBULA, g_app.show_nebulae)) {
      draw_dso_list(ctx, bounds, center, nebula_count(), nebula_equatorial,
                    nebula_name, nebula_size_deg, GColorGreen,
                    &best_d2, &best_pt, &best_r, &best_name);
    }
  }

  if (g_app.sky_mode == SKY_STARS &&
      (g_app.show_sats || g_app.show_gps ||
       (g_app.target_mode == TARGET_NAMED &&
        g_app.target_kind == TARGET_KIND_SAT))) {
    int i;
    int target = (g_app.target_mode == TARGET_NAMED &&
                  g_app.target_kind == TARGET_KIND_SAT) ?
                 (int)g_app.target_index : -1;
    for (i = 0; i < SAT_COUNT; i++) {
      int16_t px;
      int16_t py;
      bool have = false;
      bool gps = (i >= SAT_GPS_0);
      bool shown = gps ? g_app.show_gps : g_app.show_sats;
      float az;
      float alt;
      if (!shown && i != target) {
        continue;
      }
      if (iss_ready_index(i) && iss_horiz(&az, &alt)) {
        if (g_app.show_below_horizon || alt >= 0.0f) {
          have = sky_project_horiz(az, alt, &px, &py);
        }
      } else if (sat_has_pos(i)) {
        have = plot_eq(g_app.sat_ra_deg[i], g_app.sat_dec_deg[i], &px, &py);
      }
      if (!have) {
        continue;
      }
      draw_iss_mark(ctx, GPoint(px, py));
      if (px >= 0 && py >= 0 && px < bounds.size.w && py < bounds.size.h) {
        note_nearest(center, GPoint(px, py), 4, sat_name(i),
                     &best_d2, &best_pt, &best_r, &best_name);
      }
    }
  }

  if (g_app.sky_mode == SKY_STARS &&
      want_kind(TARGET_KIND_LAGRANGE, g_app.show_lagrange) &&
      lagrange_ready()) {
    draw_dso_list(ctx, bounds, center, lagrange_count(), lagrange_equatorial,
                  lagrange_name, lagrange_size_deg,
                  PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite),
                  &best_d2, &best_pt, &best_r, &best_name);
  }

  draw_constellations(ctx);
  draw_horizon(ctx);
  if (g_app.show_ecliptic) {
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorBlue, GColorWhite));
    graphics_context_set_stroke_width(ctx, 1);
    draw_sky_ring(ctx, sky_ecliptic_point);
  }
  draw_crosshair(ctx, center);

  if (g_app.show_heading) {
    draw_heading(ctx, bounds);
  }

  s_mq_max = 0;
  if (g_app.target_mode == TARGET_NAMED) {
    int16_t tx;
    int16_t ty;
    int tr = 0;
    const char *tname = NULL;
    bool on_screen = false;
    if (named_target(&tx, &ty, &tr, &tname, &on_screen)) {
      draw_leader(ctx, center, GPoint(tx, ty), on_screen ? tr : 0);
    }
    if (tname) {
      draw_name_bar(ctx, bounds, tname);
    }
  } else if (g_app.sky_mode != SKY_STARS) {
    GPoint cpt = center;
    const char *cname = NULL;
    if (nearest_constellation(center, &cpt, &cname)) {
      draw_leader(ctx, center, cpt, 0);
      draw_name_bar(ctx, bounds, cname);
    }
  } else if (best_name) {
    draw_leader(ctx, center, best_pt, best_r);
    draw_name_bar(ctx, bounds, best_name);
  }

  if (!g_app.touch_look && !g_app.compass_ok) {
    draw_overlay(ctx, bounds, "Wave watch in a figure-8");
  } else if (!g_app.has_gps) {
    draw_overlay(ctx, bounds, "Waiting for GPS...");
  }
  draw_lightbulb(ctx, bounds);
  draw_menu_arrow(ctx, bounds);
  draw_constellation_icon(ctx, bounds);
}
