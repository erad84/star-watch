#include "menu.h"

#include <stdio.h>
#include "asterism.h"
#include "catalog.h"
#include "constellation.h"
#include "deepsky.h"
#include "starwatch.h"

enum {
  LIST_SOLAR = 0,
  LIST_BRIGHT,
  LIST_FAINT,
  LIST_FAMILY,
  LIST_ASTERISM,
  LIST_DWARF,
  LIST_ASTEROID,
  LIST_CLUSTER,
  LIST_GALAXY,
  LIST_NEBULA,
  LIST_SAT
};

static const uint8_t SOLAR_DIST[] = {
  BODY_SUN, BODY_MERCURY, BODY_VENUS, BODY_MOON, BODY_MARS,
  BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE
};

#define SOLAR_COUNT ((int)(sizeof(SOLAR_DIST) / sizeof(SOLAR_DIST[0])))

static Window *s_root;
static Window *s_named;
static Window *s_families;
static Window *s_list;
static Window *s_settings;
static MenuLayer *s_root_menu;
static MenuLayer *s_named_menu;
static MenuLayer *s_families_menu;
static MenuLayer *s_list_menu;
static MenuLayer *s_settings_menu;
static int s_list_kind;
static int s_list_family;
static char s_title_buf[48];
static int s_scroll;
static int s_scroll_max;
static int s_scroll_dir = 1;
static int s_scroll_pause;
static AppTimer *s_scroll_timer;
static int s_scroll_users;

static void close_menus(void) {
  if (s_list && window_stack_contains_window(s_list)) {
    window_stack_remove(s_list, false);
  }
  if (s_families && window_stack_contains_window(s_families)) {
    window_stack_remove(s_families, false);
  }
  if (s_settings && window_stack_contains_window(s_settings)) {
    window_stack_remove(s_settings, false);
  }
  if (s_named && window_stack_contains_window(s_named)) {
    window_stack_remove(s_named, false);
  }
  if (s_root && window_stack_contains_window(s_root)) {
    window_stack_remove(s_root, true);
  }
  app_notify_target_changed();
}

static void choose_body(int body) {
  g_app.target_mode = TARGET_NAMED;
  g_app.target_kind = TARGET_KIND_BODY;
  g_app.target_index = (uint16_t)body;
  g_app.sky_mode = SKY_STARS;
  close_menus();
}

static void choose_star(int star_index) {
  g_app.target_mode = TARGET_NAMED;
  g_app.target_kind = TARGET_KIND_STAR;
  g_app.target_index = (uint16_t)star_index;
  g_app.sky_mode = SKY_STARS;
  close_menus();
}

static void choose_constell(int index) {
  g_app.target_mode = TARGET_NAMED;
  g_app.target_kind = TARGET_KIND_CONSTELL;
  g_app.target_index = (uint16_t)index;
  g_app.sky_mode = SKY_STARS;
  close_menus();
}

static void choose_asterism(int index) {
  g_app.target_mode = TARGET_NAMED;
  g_app.target_kind = TARGET_KIND_ASTERISM;
  g_app.target_index = (uint16_t)index;
  g_app.sky_mode = SKY_STARS;
  close_menus();
}

static void choose_kind(uint8_t kind, int index) {
  g_app.target_mode = TARGET_NAMED;
  g_app.target_kind = kind;
  g_app.target_index = (uint16_t)index;
  g_app.sky_mode = SKY_STARS;
  close_menus();
}

static void persist_flag(uint32_t key, bool value) {
  persist_write_int(key, value ? 1 : 0);
}

static void scroll_tick(void *context) {
  (void)context;
  s_scroll_timer = app_timer_register(80, scroll_tick, NULL);
  if (s_scroll_max <= 0) {
    return;
  }
  if (s_scroll_pause > 0) {
    s_scroll_pause--;
  } else {
    s_scroll += s_scroll_dir * 2;
    if (s_scroll <= 0) {
      s_scroll = 0;
      s_scroll_dir = 1;
      s_scroll_pause = 10;
    } else if (s_scroll >= s_scroll_max) {
      s_scroll = s_scroll_max;
      s_scroll_dir = -1;
      s_scroll_pause = 10;
    }
  }
  if (s_list_menu) {
    layer_mark_dirty(menu_layer_get_layer(s_list_menu));
  }
  if (s_named_menu) {
    layer_mark_dirty(menu_layer_get_layer(s_named_menu));
  }
  if (s_families_menu) {
    layer_mark_dirty(menu_layer_get_layer(s_families_menu));
  }
}

static void scroll_retain(void) {
  if (s_scroll_users++ == 0) {
    s_scroll_timer = app_timer_register(80, scroll_tick, NULL);
  }
  s_scroll = 0;
  s_scroll_dir = 1;
  s_scroll_pause = 8;
  s_scroll_max = 0;
}

static void scroll_release(void) {
  if (--s_scroll_users <= 0) {
    s_scroll_users = 0;
    if (s_scroll_timer) {
      app_timer_cancel(s_scroll_timer);
      s_scroll_timer = NULL;
    }
  }
}

static void reset_scroll(MenuLayer *menu, MenuIndex new_index, MenuIndex old_index, void *context) {
  (void)menu;
  (void)new_index;
  (void)old_index;
  (void)context;
  s_scroll = 0;
  s_scroll_dir = 1;
  s_scroll_pause = 8;
  s_scroll_max = 0;
}

static void draw_scroll_title(GContext *ctx, const Layer *cell_layer, const char *title, int right_pad) {
  GRect b = layer_get_bounds(cell_layer);
#ifdef PBL_PLATFORM_FLINT
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const int th = 22;
#else
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const int th = 28;
#endif
  GSize sz;
  int avail;
  bool hi;
  GColor fg;
  if (!title) {
    title = "";
  }
  sz = graphics_text_layout_get_content_size(
      title, font, GRect(0, 0, 1000, 32),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  avail = b.size.w - 8 - right_pad;
#ifdef PBL_ROUND
  avail -= 24;
#endif
  if (avail < 32) {
    avail = 32;
  }
  hi = menu_cell_layer_is_highlighted(cell_layer);
  fg = hi ? PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack) : GColorWhite;
  graphics_context_set_text_color(ctx, fg);
  if (sz.w <= avail) {
    graphics_draw_text(ctx, title, font,
                       GRect(4, (b.size.h - th) / 2, avail, th),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    return;
  }
  if (hi) {
    s_scroll_max = sz.w - avail;
    if (s_scroll > s_scroll_max) {
      s_scroll = s_scroll_max;
    }
    graphics_draw_text(ctx, title, font,
                       GRect(4 - s_scroll, (b.size.h - th) / 2, sz.w + 8, th),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, title, font,
                       GRect(4, (b.size.h - th) / 2, sz.w + 8, th),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

static void style_menu(MenuLayer *menu) {
  menu_layer_set_normal_colors(menu, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(menu,
      PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorWhite),
      PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
}

static uint16_t one_section(MenuLayer *layer, void *ctx) {
  (void)layer;
  (void)ctx;
  return 1;
}

static int16_t header_height(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)section;
  (void)ctx;
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static int16_t cell_height(MenuLayer *layer, MenuIndex *index, void *ctx) {
  (void)layer;
  (void)index;
  (void)ctx;
#ifdef PBL_ROUND
  return 44;
#elif defined(PBL_PLATFORM_FLINT)
  return 32;
#else
  return 36;
#endif
}

static void draw_header_text(GContext *ctx, const Layer *cell_layer, const char *title) {
  GRect bounds = layer_get_bounds(cell_layer);
#ifdef PBL_ROUND
  bounds.origin.x += 18;
  bounds.size.w -= 36;
#elif defined(PBL_PLATFORM_FLINT)
  bounds.origin.x += 2;
  bounds.size.w -= 4;
#endif
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_checkbox(GContext *ctx, GRect bounds, bool on) {
  int box = 14;
#ifdef PBL_ROUND
  int bx = bounds.size.w - box - 22;
#elif defined(PBL_PLATFORM_FLINT)
  int bx = bounds.size.w - box - 4;
#else
  int bx = bounds.size.w - box - 8;
#endif
  int by = (bounds.size.h - box) / 2;
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, GRect(bx, by, box, box));
  if (on) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(bx + 3, by + 3, box - 6, box - 6), 0, GCornerNone);
  }
}

static uint16_t root_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)section;
  (void)ctx;
  return 4;
}

static void root_header(GContext *ctx, const Layer *cell_layer, uint16_t section, void *callback_context) {
  (void)section;
  (void)callback_context;
  draw_header_text(ctx, cell_layer, "Targeting");
}

static void root_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *callback_context) {
  (void)callback_context;
  if (index->row == 0) {
    draw_scroll_title(ctx, cell_layer, "Object Targeting", 0);
  } else if (index->row == 1) {
    draw_scroll_title(ctx, cell_layer, "Settings", 0);
  } else if (index->row == 2) {
    snprintf(s_title_buf, sizeof(s_title_buf), "Compass %u%%",
             (unsigned)g_app.compass_pct);
    draw_scroll_title(ctx, cell_layer, s_title_buf, 0);
  } else {
    snprintf(s_title_buf, sizeof(s_title_buf), "GPS %u%%",
             (unsigned)g_app.gps_pct);
    draw_scroll_title(ctx, cell_layer, s_title_buf, 0);
  }
}

static void root_select(MenuLayer *layer, MenuIndex *index, void *ctx) {
  (void)layer;
  (void)ctx;
  if (index->row == 0) {
    window_stack_push(s_named, true);
    return;
  }
  if (index->row == 1) {
    window_stack_push(s_settings, true);
    return;
  }
  if (index->row == 2) {
    app_open_calibration();
  }
}

#define SETTINGS_COUNT 16

static const char *const SETTING_TITLES[SETTINGS_COUNT] = {
  "Cardinals",
  "Heading",
  "Below horizon",
  "Sun",
  "Moon",
  "Planets",
  "Bright stars",
  "Faint stars",
  "Satellites",
  "Dwarf planets",
  "Asteroids",
  "Bright clusters",
  "Galaxies",
  "Nebulae",
  "Constellations",
  "Asterisms"
};

static const uint32_t SETTING_KEYS[SETTINGS_COUNT] = {
  PERSIST_CARDINALS, PERSIST_HEADING, PERSIST_BELOW, PERSIST_SUN,
  PERSIST_MOON, PERSIST_PLANETS, PERSIST_BRIGHT, PERSIST_FAINT,
  PERSIST_SATS, PERSIST_DWARFS, PERSIST_ASTEROIDS, PERSIST_CLUSTERS,
  PERSIST_GALAXIES, PERSIST_NEBULAE, PERSIST_CONSTELLS, PERSIST_ASTERISMS
};

static bool *setting_flag(int row) {
  switch (row) {
    case 0: return &g_app.show_cardinals;
    case 1: return &g_app.show_heading;
    case 2: return &g_app.show_below_horizon;
    case 3: return &g_app.show_sun;
    case 4: return &g_app.show_moon;
    case 5: return &g_app.show_planets;
    case 6: return &g_app.show_bright_stars;
    case 7: return &g_app.show_faint_stars;
    case 8: return &g_app.show_sats;
    case 9: return &g_app.show_dwarfs;
    case 10: return &g_app.show_asteroids;
    case 11: return &g_app.show_clusters;
    case 12: return &g_app.show_galaxies;
    case 13: return &g_app.show_nebulae;
    case 14: return &g_app.show_constellations;
    case 15: return &g_app.show_asterisms;
    default: return NULL;
  }
}

static uint16_t settings_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)section;
  (void)ctx;
  return SETTINGS_COUNT;
}

static void settings_header(GContext *ctx, const Layer *cell_layer, uint16_t section, void *callback_context) {
  (void)section;
  (void)callback_context;
  draw_header_text(ctx, cell_layer, "Settings");
}

static void settings_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *callback_context) {
  bool *flag;
  (void)callback_context;
  if (index->row >= SETTINGS_COUNT) {
    return;
  }
  flag = setting_flag((int)index->row);
  draw_scroll_title(ctx, cell_layer, SETTING_TITLES[index->row], 26);
  draw_checkbox(ctx, layer_get_bounds(cell_layer), flag && *flag);
}

static void settings_select(MenuLayer *layer, MenuIndex *index, void *ctx) {
  bool *flag;
  (void)ctx;
  if (index->row >= SETTINGS_COUNT) {
    return;
  }
  flag = setting_flag((int)index->row);
  if (!flag) {
    return;
  }
  *flag = !*flag;
  persist_flag(SETTING_KEYS[index->row], *flag);
  menu_layer_reload_data(layer);
}

static uint16_t named_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)section;
  (void)ctx;
  return 11;
}

static void named_header(GContext *ctx, const Layer *cell_layer, uint16_t section, void *callback_context) {
  (void)section;
  (void)callback_context;
  draw_header_text(ctx, cell_layer, "Object Targeting");
}

static void named_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *callback_context) {
  (void)callback_context;
  switch (index->row) {
    case 0:
      snprintf(s_title_buf, sizeof(s_title_buf), "Solar System - %d", SOLAR_COUNT);
      break;
    case 1:
      snprintf(s_title_buf, sizeof(s_title_buf), "Dwarf Planets - %d", DWARF_COUNT);
      break;
    case 2:
      snprintf(s_title_buf, sizeof(s_title_buf), "Asteroids - %d", ASTEROID_COUNT);
      break;
    case 3:
      snprintf(s_title_buf, sizeof(s_title_buf), "Satellites - %d", SAT_COUNT);
      break;
    case 4:
      snprintf(s_title_buf, sizeof(s_title_buf), "Bright Stars - %d", catalog_bright_count());
      break;
    case 5:
      snprintf(s_title_buf, sizeof(s_title_buf), "Faint Stars - %d", catalog_faint_count());
      break;
    case 6:
      snprintf(s_title_buf, sizeof(s_title_buf), "Bright Clusters - %d", cluster_count());
      break;
    case 7:
      snprintf(s_title_buf, sizeof(s_title_buf), "Galaxies - %d", galaxy_count());
      break;
    case 8:
      snprintf(s_title_buf, sizeof(s_title_buf), "Nebulae - %d", nebula_count());
      break;
    case 9:
      snprintf(s_title_buf, sizeof(s_title_buf), "Constellations - %d", constellation_count());
      break;
    default:
      snprintf(s_title_buf, sizeof(s_title_buf), "Asterisms - %d", asterism_count());
      break;
  }
  draw_scroll_title(ctx, cell_layer, s_title_buf, 0);
}

static void named_select(MenuLayer *layer, MenuIndex *index, void *ctx) {
  (void)layer;
  (void)ctx;
  switch (index->row) {
    case 0:
      s_list_kind = LIST_SOLAR;
      break;
    case 1:
      s_list_kind = LIST_DWARF;
      break;
    case 2:
      s_list_kind = LIST_ASTEROID;
      break;
    case 3:
      s_list_kind = LIST_SAT;
      break;
    case 4:
      s_list_kind = LIST_BRIGHT;
      break;
    case 5:
      s_list_kind = LIST_FAINT;
      break;
    case 6:
      s_list_kind = LIST_CLUSTER;
      break;
    case 7:
      s_list_kind = LIST_GALAXY;
      break;
    case 8:
      s_list_kind = LIST_NEBULA;
      break;
    case 9:
      window_stack_push(s_families, true);
      return;
    default:
      s_list_kind = LIST_ASTERISM;
      break;
  }
  window_stack_push(s_list, true);
}

static uint16_t family_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)section;
  (void)ctx;
  return (uint16_t)constellation_family_count();
}

static void family_header(GContext *ctx, const Layer *cell_layer, uint16_t section, void *callback_context) {
  (void)section;
  (void)callback_context;
  draw_header_text(ctx, cell_layer, "Constellations");
}

static void family_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *callback_context) {
  const char *name = constellation_family_name((int)index->row);
  (void)callback_context;
  snprintf(s_title_buf, sizeof(s_title_buf), "%s - %d",
           name ? name : "Family",
           constellation_family_member_count((int)index->row));
  draw_scroll_title(ctx, cell_layer, s_title_buf, 0);
}

static void family_select(MenuLayer *layer, MenuIndex *index, void *ctx) {
  (void)layer;
  (void)ctx;
  s_list_kind = LIST_FAMILY;
  s_list_family = (int)index->row;
  window_stack_push(s_list, true);
}

static uint16_t list_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)section;
  (void)ctx;
  if (s_list_kind == LIST_SOLAR) {
    return (uint16_t)SOLAR_COUNT;
  }
  if (s_list_kind == LIST_BRIGHT) {
    return (uint16_t)catalog_bright_count();
  }
  if (s_list_kind == LIST_FAINT) {
    return (uint16_t)catalog_faint_count();
  }
  if (s_list_kind == LIST_ASTERISM) {
    return (uint16_t)asterism_count();
  }
  if (s_list_kind == LIST_DWARF) {
    return (uint16_t)DWARF_COUNT;
  }
  if (s_list_kind == LIST_ASTEROID) {
    return (uint16_t)ASTEROID_COUNT;
  }
  if (s_list_kind == LIST_CLUSTER) {
    return (uint16_t)cluster_count();
  }
  if (s_list_kind == LIST_GALAXY) {
    return (uint16_t)galaxy_count();
  }
  if (s_list_kind == LIST_NEBULA) {
    return (uint16_t)nebula_count();
  }
  if (s_list_kind == LIST_SAT) {
    return (uint16_t)SAT_COUNT;
  }
  return (uint16_t)constellation_family_member_count(s_list_family);
}

static void list_header(GContext *ctx, const Layer *cell_layer, uint16_t section, void *callback_context) {
  const char *title = "Constellations";
  (void)section;
  (void)callback_context;
  if (s_list_kind == LIST_SOLAR) {
    title = "Solar System";
  } else if (s_list_kind == LIST_BRIGHT) {
    title = "Bright Stars";
  } else if (s_list_kind == LIST_FAINT) {
    title = "Faint Stars";
  } else if (s_list_kind == LIST_ASTERISM) {
    title = "Asterisms";
  } else if (s_list_kind == LIST_DWARF) {
    title = "Dwarf Planets";
  } else if (s_list_kind == LIST_ASTEROID) {
    title = "Asteroids";
  } else if (s_list_kind == LIST_CLUSTER) {
    title = "Bright Clusters";
  } else if (s_list_kind == LIST_GALAXY) {
    title = "Galaxies";
  } else if (s_list_kind == LIST_NEBULA) {
    title = "Nebulae";
  } else if (s_list_kind == LIST_SAT) {
    title = "Satellites";
  } else {
    title = constellation_family_name(s_list_family);
    if (!title) {
      title = "Constellations";
    }
  }
  draw_header_text(ctx, cell_layer, title);
}

static void list_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index, void *callback_context) {
  const char *title = "";
  (void)callback_context;
  if (s_list_kind == LIST_SOLAR) {
    title = BODY_NAMES[SOLAR_DIST[index->row]];
  } else if (s_list_kind == LIST_BRIGHT) {
    int star_i = catalog_bright_star_index((int)index->row);
    const PackedStar *s = catalog_star(star_i);
    title = (s && catalog_name(s->name_id)) ? catalog_name(s->name_id) : "Star";
  } else if (s_list_kind == LIST_FAINT) {
    int star_i = catalog_faint_star_index((int)index->row);
    const PackedStar *s = catalog_star(star_i);
    title = (s && catalog_name(s->name_id)) ? catalog_name(s->name_id) : "Star";
  } else if (s_list_kind == LIST_ASTERISM) {
    title = asterism_name((int)index->row);
    if (!title) {
      title = "Asterism";
    }
  } else if (s_list_kind == LIST_DWARF) {
    title = DWARF_NAMES[index->row];
  } else if (s_list_kind == LIST_ASTEROID) {
    title = ASTEROID_NAMES[index->row];
  } else if (s_list_kind == LIST_CLUSTER) {
    title = cluster_name((int)index->row);
  } else if (s_list_kind == LIST_GALAXY) {
    title = galaxy_name((int)index->row);
  } else if (s_list_kind == LIST_NEBULA) {
    title = nebula_name((int)index->row);
  } else if (s_list_kind == LIST_SAT) {
    title = SAT_NAMES[index->row];
  } else {
    title = constellation_name(constellation_family_member(s_list_family, (int)index->row));
    if (!title) {
      title = "Constellation";
    }
  }
  draw_scroll_title(ctx, cell_layer, title, 0);
}

static void list_select(MenuLayer *layer, MenuIndex *index, void *ctx) {
  (void)layer;
  (void)ctx;
  if (s_list_kind == LIST_SOLAR) {
    choose_body(SOLAR_DIST[index->row]);
  } else if (s_list_kind == LIST_BRIGHT) {
    choose_star(catalog_bright_star_index((int)index->row));
  } else if (s_list_kind == LIST_FAINT) {
    choose_star(catalog_faint_star_index((int)index->row));
  } else if (s_list_kind == LIST_ASTERISM) {
    choose_asterism((int)index->row);
  } else if (s_list_kind == LIST_DWARF) {
    choose_kind(TARGET_KIND_DWARF, (int)index->row);
  } else if (s_list_kind == LIST_ASTEROID) {
    choose_kind(TARGET_KIND_ASTEROID, (int)index->row);
  } else if (s_list_kind == LIST_CLUSTER) {
    choose_kind(TARGET_KIND_CLUSTER, (int)index->row);
  } else if (s_list_kind == LIST_GALAXY) {
    choose_kind(TARGET_KIND_GALAXY, (int)index->row);
  } else if (s_list_kind == LIST_NEBULA) {
    choose_kind(TARGET_KIND_NEBULA, (int)index->row);
  } else if (s_list_kind == LIST_SAT) {
    choose_kind(TARGET_KIND_SAT, (int)index->row);
  } else {
    choose_constell(constellation_family_member(s_list_family, (int)index->row));
  }
}

static MenuLayer *make_menu(Window *window, const MenuLayerCallbacks *cbs) {
  Layer *root = window_get_root_layer(window);
  MenuLayer *menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(menu, NULL, *cbs);
  menu_layer_set_click_config_onto_window(menu, window);
  style_menu(menu);
#ifdef PBL_ROUND
  menu_layer_set_center_focused(menu, true);
#endif
  layer_add_child(root, menu_layer_get_layer(menu));
  return menu;
}

static void root_load(Window *window) {
  static const MenuLayerCallbacks cbs = {
    .get_num_sections = one_section,
    .get_num_rows = root_rows,
    .get_header_height = header_height,
    .get_cell_height = cell_height,
    .draw_header = root_header,
    .draw_row = root_row,
    .select_click = root_select,
    .selection_changed = reset_scroll,
  };
  s_root_menu = make_menu(window, &cbs);
  scroll_retain();
}

static void root_unload(Window *window) {
  (void)window;
  scroll_release();
  menu_layer_destroy(s_root_menu);
  s_root_menu = NULL;
}

static void named_load(Window *window) {
  static const MenuLayerCallbacks cbs = {
    .get_num_sections = one_section,
    .get_num_rows = named_rows,
    .get_header_height = header_height,
    .get_cell_height = cell_height,
    .draw_header = named_header,
    .draw_row = named_row,
    .select_click = named_select,
    .selection_changed = reset_scroll,
  };
  s_named_menu = make_menu(window, &cbs);
  scroll_retain();
}

static void named_unload(Window *window) {
  (void)window;
  scroll_release();
  menu_layer_destroy(s_named_menu);
  s_named_menu = NULL;
}

static void families_load(Window *window) {
  static const MenuLayerCallbacks cbs = {
    .get_num_sections = one_section,
    .get_num_rows = family_rows,
    .get_header_height = header_height,
    .get_cell_height = cell_height,
    .draw_header = family_header,
    .draw_row = family_row,
    .select_click = family_select,
    .selection_changed = reset_scroll,
  };
  s_families_menu = make_menu(window, &cbs);
  scroll_retain();
}

static void families_unload(Window *window) {
  (void)window;
  scroll_release();
  menu_layer_destroy(s_families_menu);
  s_families_menu = NULL;
}

static void list_load(Window *window) {
  static const MenuLayerCallbacks cbs = {
    .get_num_sections = one_section,
    .get_num_rows = list_rows,
    .get_header_height = header_height,
    .get_cell_height = cell_height,
    .draw_header = list_header,
    .draw_row = list_row,
    .select_click = list_select,
    .selection_changed = reset_scroll,
  };
  s_list_menu = make_menu(window, &cbs);
  scroll_retain();
}

static void list_unload(Window *window) {
  (void)window;
  scroll_release();
  menu_layer_destroy(s_list_menu);
  s_list_menu = NULL;
}

static void settings_load(Window *window) {
  static const MenuLayerCallbacks cbs = {
    .get_num_sections = one_section,
    .get_num_rows = settings_rows,
    .get_header_height = header_height,
    .get_cell_height = cell_height,
    .draw_header = settings_header,
    .draw_row = settings_row,
    .select_click = settings_select,
    .selection_changed = reset_scroll,
  };
  s_settings_menu = make_menu(window, &cbs);
  scroll_retain();
}

static void settings_unload(Window *window) {
  (void)window;
  scroll_release();
  menu_layer_destroy(s_settings_menu);
  s_settings_menu = NULL;
}

void menu_init(void) {
  s_root = window_create();
  window_set_background_color(s_root, GColorBlack);
  window_set_window_handlers(s_root, (WindowHandlers) {
    .load = root_load,
    .unload = root_unload,
  });

  s_named = window_create();
  window_set_background_color(s_named, GColorBlack);
  window_set_window_handlers(s_named, (WindowHandlers) {
    .load = named_load,
    .unload = named_unload,
  });

  s_families = window_create();
  window_set_background_color(s_families, GColorBlack);
  window_set_window_handlers(s_families, (WindowHandlers) {
    .load = families_load,
    .unload = families_unload,
  });

  s_list = window_create();
  window_set_background_color(s_list, GColorBlack);
  window_set_window_handlers(s_list, (WindowHandlers) {
    .load = list_load,
    .unload = list_unload,
  });

  s_settings = window_create();
  window_set_background_color(s_settings, GColorBlack);
  window_set_window_handlers(s_settings, (WindowHandlers) {
    .load = settings_load,
    .unload = settings_unload,
  });
}

void menu_deinit(void) {
  if (s_list) {
    if (window_stack_contains_window(s_list)) {
      window_stack_remove(s_list, false);
    }
    window_destroy(s_list);
    s_list = NULL;
  }
  if (s_settings) {
    if (window_stack_contains_window(s_settings)) {
      window_stack_remove(s_settings, false);
    }
    window_destroy(s_settings);
    s_settings = NULL;
  }
  if (s_families) {
    if (window_stack_contains_window(s_families)) {
      window_stack_remove(s_families, false);
    }
    window_destroy(s_families);
    s_families = NULL;
  }
  if (s_named) {
    if (window_stack_contains_window(s_named)) {
      window_stack_remove(s_named, false);
    }
    window_destroy(s_named);
    s_named = NULL;
  }
  if (s_root) {
    if (window_stack_contains_window(s_root)) {
      window_stack_remove(s_root, false);
    }
    window_destroy(s_root);
    s_root = NULL;
  }
}

void menu_open(void) {
  if (!s_root) {
    return;
  }
  window_stack_push(s_root, true);
}

void menu_refresh(void) {
  if (s_root_menu) {
    menu_layer_reload_data(s_root_menu);
  }
}
