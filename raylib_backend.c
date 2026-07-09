#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "main.h"
#include "meta.h"
#include "peanut_gb.h"

#define PC_VRAM_TILE_SCALE 2.0f
#define PROFILE_PANEL_WIDTH 430
#define PROFILE_PANEL_MARGIN 10

float camera_distance = 10.0f;
Camera3D camera;

static int clamp_int(int value, int min, int max){
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static uint32_t selected_tile_hash(void){
    if (selected_tile < 0 || selected_tile >= VRAM_TILE_COUNT) return 0;
    return tiles_on_vram[selected_tile].hash;
}

static int meta_count(meta_t *meta){
    int count = 0;
    for (meta_t *m = meta; m != NULL; m = m->next) count++;
    return count;
}

static int profile_effective_selection_count(profile_editor_t *ed){
    return ed->selected_count > 0 ? ed->selected_count : 1;
}

static int profile_used_tile_count(profile_editor_t *ed){
    int count = 0;
    for (int i=0; i<VRAM_TILE_COUNT; i++) if (ed->used_tiles[i]) count++;
    return count;
}


static bool profile_tile_is_selected(profile_editor_t *ed, int tile){
    if (tile < 0 || tile >= VRAM_TILE_COUNT) return false;
    return ed->selected_tiles[tile];
}


static void profile_clear_selection(profile_editor_t *ed){
    for (int i=0; i<VRAM_TILE_COUNT; i++) ed->selected_tiles[i] = false;
    ed->selected_count = 0;
}


static void profile_add_tile_to_selection(profile_editor_t *ed, int tile){
    if (tile < 0 || tile >= VRAM_TILE_COUNT) return;
    if (!ed->selected_tiles[tile]){
        ed->selected_tiles[tile] = true;
        ed->selected_count++;
    }
}


static void profile_select_single_tile(profile_editor_t *ed, int tile){
    profile_clear_selection(ed);
    profile_add_tile_to_selection(ed, tile);
    ed->selection_anchor = tile;
}


static void profile_toggle_tile_selection(profile_editor_t *ed, int tile){
    if (tile < 0 || tile >= VRAM_TILE_COUNT) return;
    if (ed->selected_tiles[tile]){
        ed->selected_tiles[tile] = false;
        if (ed->selected_count > 0) ed->selected_count--;
    }
    else{
        ed->selected_tiles[tile] = true;
        ed->selected_count++;
    }
    ed->selection_anchor = tile;
}


static void profile_select_tile_range(profile_editor_t *ed, int tile){
    if (tile < 0 || tile >= VRAM_TILE_COUNT) return;
    int start = ed->selection_anchor;
    if (start < 0 || start >= VRAM_TILE_COUNT) start = tile;
    int end = tile;
    if (start > end){
        int tmp = start;
        start = end;
        end = tmp;
    }
    profile_clear_selection(ed);
    for (int i=start; i<=end; i++) profile_add_tile_to_selection(ed, i);
}


static void profile_select_tile_rect(profile_editor_t *ed, int start_tile, int end_tile, bool add_to_existing){
    if (start_tile < 0 || start_tile >= VRAM_TILE_COUNT) return;
    if (end_tile < 0 || end_tile >= VRAM_TILE_COUNT) return;

    int start_col = start_tile % VRAM_INSPECTOR_WIDTH;
    int start_row = start_tile / VRAM_INSPECTOR_WIDTH;
    int end_col = end_tile % VRAM_INSPECTOR_WIDTH;
    int end_row = end_tile / VRAM_INSPECTOR_WIDTH;

    int min_col = start_col < end_col ? start_col : end_col;
    int max_col = start_col > end_col ? start_col : end_col;
    int min_row = start_row < end_row ? start_row : end_row;
    int max_row = start_row > end_row ? start_row : end_row;

    if (!add_to_existing) profile_clear_selection(ed);

    for (int row=min_row; row<=max_row; row++){
        for (int col=min_col; col<=max_col; col++){
            int tile = row * VRAM_INSPECTOR_WIDTH + col;
            if (tile >= 0 && tile < VRAM_TILE_COUNT) profile_add_tile_to_selection(ed, tile);
        }
    }
    ed->selection_anchor = start_tile;
}


static bool profile_tile_in_rect(int tile, int start_tile, int end_tile){
    if (tile < 0 || tile >= VRAM_TILE_COUNT) return false;
    if (start_tile < 0 || start_tile >= VRAM_TILE_COUNT) return false;
    if (end_tile < 0 || end_tile >= VRAM_TILE_COUNT) return false;

    int tile_col = tile % VRAM_INSPECTOR_WIDTH;
    int tile_row = tile / VRAM_INSPECTOR_WIDTH;
    int start_col = start_tile % VRAM_INSPECTOR_WIDTH;
    int start_row = start_tile / VRAM_INSPECTOR_WIDTH;
    int end_col = end_tile % VRAM_INSPECTOR_WIDTH;
    int end_row = end_tile / VRAM_INSPECTOR_WIDTH;

    int min_col = start_col < end_col ? start_col : end_col;
    int max_col = start_col > end_col ? start_col : end_col;
    int min_row = start_row < end_row ? start_row : end_row;
    int max_row = start_row > end_row ? start_row : end_row;

    return tile_col >= min_col && tile_col <= max_col && tile_row >= min_row && tile_row <= max_row;
}


static bool profile_has_tile_in_selection(profile_editor_t *ed, int tile){
    if (ed->selected_count <= 0) return tile == selected_tile;
    return profile_tile_is_selected(ed, tile);
}


static Color meta_target_color(meta_t *m, profile_color_target_t target){
    if (m == NULL) return BLACK;
    switch (target){
        case PROFILE_COLOR_WIN: return m->win_color;
        case PROFILE_COLOR_OBJ: return m->obj_color;
        case PROFILE_COLOR_BG:
        default: return m->bg_color;
    }
}

static const char *target_label(profile_color_target_t target){
    switch (target){
        case PROFILE_COLOR_WIN: return "WIN";
        case PROFILE_COLOR_OBJ: return "OBJ";
        case PROFILE_COLOR_BG:
        default: return "BG";
    }
}

static int profile_bg_tile_index(gb_s *gb, uint8_t idx){
    if (gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT) return idx;
    return 128 + ((idx + 128) & 0xFF);
}

static void profile_mark_used_tile(profile_editor_t *ed, int tile){
    if (tile >= 0 && tile < VRAM_TILE_COUNT) ed->used_tiles[tile] = true;
}

static void profile_update_used_tiles(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    gb_s *gb = &app->gb;
    for (int i=0; i<VRAM_TILE_COUNT; i++) ed->used_tiles[i] = false;

    if (gb->hram_io[IO_LCDC] & LCDC_BG_ENABLE){
        uint16_t bg_map = (gb->hram_io[IO_LCDC] & LCDC_BG_MAP) ? VRAM_BMAP_2 : VRAM_BMAP_1;
        for (int y=0; y<LCD_HEIGHT; y+=8){
            for (int x=0; x<LCD_WIDTH; x+=8){
                uint8_t bg_x = (uint8_t)(x + gb->hram_io[IO_SCX]);
                uint8_t bg_y = (uint8_t)(y + gb->hram_io[IO_SCY]);
                uint16_t map_i = bg_map + (bg_y >> 3) * 32 + (bg_x >> 3);
                profile_mark_used_tile(ed, profile_bg_tile_index(gb, gb->vram[map_i]));
            }
        }
    }

    if ((gb->hram_io[IO_LCDC] & LCDC_WINDOW_ENABLE) && gb->hram_io[IO_WY] < LCD_HEIGHT && gb->hram_io[IO_WX] <= 166){
        uint16_t win_map = (gb->hram_io[IO_LCDC] & LCDC_WINDOW_MAP) ? VRAM_BMAP_2 : VRAM_BMAP_1;
        int wx = (int)gb->hram_io[IO_WX] - 7;
        int wy = gb->hram_io[IO_WY];
        if (wx < LCD_WIDTH){
            int start_x = wx < 0 ? 0 : wx;
            for (int y=wy; y<LCD_HEIGHT; y+=8){
                for (int x=start_x; x<LCD_WIDTH; x+=8){
                    int win_x = x - wx;
                    int win_y = y - wy;
                    uint16_t map_i = win_map + ((win_y >> 3) & 31) * 32 + ((win_x >> 3) & 31);
                    profile_mark_used_tile(ed, profile_bg_tile_index(gb, gb->vram[map_i]));
                }
            }
        }
    }

    if (gb->hram_io[IO_LCDC] & LCDC_OBJ_ENABLE){
        bool tall = (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE) != 0;
        for (int i=0; i<40; i++){
            int oy = gb->oam[i * 4];
            int ox = gb->oam[i * 4 + 1];
            int tile = gb->oam[i * 4 + 2];
            if (ox == 0 || ox >= 168 || oy == 0 || oy >= 160) continue;
            if (tall){
                int base = tile & 0xFE;
                profile_mark_used_tile(ed, base);
                profile_mark_used_tile(ed, base + 1);
            }
            else profile_mark_used_tile(ed, tile);
        }
    }
}

static meta_t *ensure_meta_for_hash(app_state *app, uint32_t hash){
    if (hash == 0) return NULL;
    meta_t *m = get_meta(app->meta, hash);
    if (m == NULL){
        set_meta(&app->meta, hash, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        m = get_meta(app->meta, hash);
    }
    return m;
}

static meta_t *ensure_selected_meta(app_state *app){
    return ensure_meta_for_hash(app, selected_tile_hash());
}

static void profile_set_status(app_state *app, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->profile_editor.status, sizeof(app->profile_editor.status), fmt, args);
    va_end(args);
}

static void profile_set_last_change(app_state *app, const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->profile_editor.last_change, sizeof(app->profile_editor.last_change), fmt, args);
    va_end(args);
}

static void profile_capture_meta(profile_history_entry_t *entry, meta_t *meta, const char *label){
    if (entry == NULL) return;
    memset(entry, 0, sizeof(*entry));
    if (label != NULL) snprintf(entry->label, sizeof(entry->label), "%s", label);
    for (meta_t *m = meta; m != NULL && entry->count < PROFILE_HISTORY_MAX_META; m = m->next){
        profile_meta_state_t *e = &entry->entries[entry->count++];
        e->tile_hash = m->tile_hash;
        e->bg_color = m->bg_color;
        e->win_color = m->win_color;
        e->obj_color = m->obj_color;
        e->bg_for_z = m->bg_for_z;
        e->bg_back_z = m->bg_back_z;
        e->win_z = m->win_z;
        e->obj_z = m->obj_z;
        e->obj_behind_z = m->obj_behind_z;
        e->flags = m->flags;
    }
}

static void profile_restore_meta(app_state *app, profile_history_entry_t *entry){
    if (app == NULL || entry == NULL) return;
    free_meta(app->meta);
    app->meta = NULL;
    for (int i=0; i<entry->count; i++){
        profile_meta_state_t *e = &entry->entries[i];
        set_meta(&app->meta, e->tile_hash, &e->bg_color, &e->win_color, &e->obj_color,
            &e->bg_for_z, &e->bg_back_z, &e->win_z, &e->obj_z, &e->obj_behind_z);
        meta_t *m = get_meta(app->meta, e->tile_hash);
        if (m != NULL) meta_set_flags(m, e->flags);
    }
}

static void profile_history_push(profile_history_entry_t *stack, int *count, meta_t *meta, const char *label){
    if (stack == NULL || count == NULL) return;
    if (*count >= PROFILE_HISTORY_MAX){
        memmove(&stack[0], &stack[1], sizeof(profile_history_entry_t) * (PROFILE_HISTORY_MAX - 1));
        *count = PROFILE_HISTORY_MAX - 1;
    }
    profile_capture_meta(&stack[*count], meta, label);
    (*count)++;
}

void profile_push_undo(app_state *app, const char *label){
    if (app == NULL) return;
    profile_editor_t *ed = &app->profile_editor;
    profile_history_push(ed->undo_stack, &ed->undo_count, app->meta, label);
    ed->redo_count = 0;
}

void profile_mark_dirty(app_state *app, const char *label){
    if (app == NULL) return;
    profile_editor_t *ed = &app->profile_editor;
    ed->dirty = true;
    ed->unsaved_changes++;
    ed->confirm_load = 0;
    if (label != NULL && label[0] != '\0') profile_set_last_change(app, "%s", label);
}

void profile_mark_saved(app_state *app){
    if (app == NULL) return;
    app->profile_editor.dirty = false;
    app->profile_editor.unsaved_changes = 0;
    app->profile_editor.confirm_load = 0;
    profile_set_status(app, "profile saved");
}

void profile_after_load(app_state *app){
    if (app == NULL) return;
    profile_editor_t *ed = &app->profile_editor;
    ed->dirty = false;
    ed->unsaved_changes = 0;
    ed->confirm_load = 0;
    ed->undo_count = 0;
    ed->redo_count = 0;
    ed->synced_hash = 0;
    profile_set_status(app, "profile loaded");
}

static void profile_undo(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    if (ed->undo_count <= 0){
        profile_set_status(app, "nothing to undo");
        return;
    }
    profile_history_push(ed->redo_stack, &ed->redo_count, app->meta, "redo point");
    profile_history_entry_t entry = ed->undo_stack[--ed->undo_count];
    profile_restore_meta(app, &entry);
    ed->dirty = true;
    ed->unsaved_changes++;
    ed->synced_hash = 0;
    profile_set_status(app, "undo: %s", entry.label[0] ? entry.label : "change");
    profile_set_last_change(app, "undo: %s", entry.label[0] ? entry.label : "change");
}

static void profile_redo(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    if (ed->redo_count <= 0){
        profile_set_status(app, "nothing to redo");
        return;
    }
    profile_history_push(ed->undo_stack, &ed->undo_count, app->meta, "undo point");
    profile_history_entry_t entry = ed->redo_stack[--ed->redo_count];
    profile_restore_meta(app, &entry);
    ed->dirty = true;
    ed->unsaved_changes++;
    ed->synced_hash = 0;
    profile_set_status(app, "redo");
    profile_set_last_change(app, "redo");
}

static void profile_sync_color(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    uint32_t hash = selected_tile_hash();
    if (ed->synced_hash == hash && ed->synced_target == ed->color_target) return;

    meta_t *m = get_meta(app->meta, hash);
    Color c = meta_target_color(m, ed->color_target);
    ed->color_r = c.r;
    ed->color_g = c.g;
    ed->color_b = c.b;
    ed->synced_hash = hash;
    ed->synced_target = ed->color_target;
}

static void profile_apply_color_to_hash(app_state *app, uint32_t hash, Color c){
    switch (app->profile_editor.color_target){
        case PROFILE_COLOR_WIN:
            set_meta(&app->meta, hash, NULL, &c, NULL, NULL, NULL, NULL, NULL, NULL);
            break;
        case PROFILE_COLOR_OBJ:
            set_meta(&app->meta, hash, NULL, NULL, &c, NULL, NULL, NULL, NULL, NULL);
            break;
        case PROFILE_COLOR_BG:
        default:
            set_meta(&app->meta, hash, &c, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            break;
    }
}

static int profile_apply_color(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    Color c = (Color){
        (unsigned char)clamp_int(ed->color_r, 0, 255),
        (unsigned char)clamp_int(ed->color_g, 0, 255),
        (unsigned char)clamp_int(ed->color_b, 0, 255),
        255
    };

    profile_push_undo(app, "color change");
    int changed = 0;
    if (ed->selected_count > 0){
        for (int i=0; i<VRAM_TILE_COUNT; i++){
            if (!ed->selected_tiles[i]) continue;
            profile_apply_color_to_hash(app, tiles_on_vram[i].hash, c);
            changed++;
        }
    }
    else{
        profile_apply_color_to_hash(app, selected_tile_hash(), c);
        changed = 1;
    }

    profile_set_status(app, "%s color pending value applied to %d tile%s", target_label(ed->color_target), changed, changed == 1 ? "" : "s");
    profile_set_last_change(app, "%s color RGB %d %d %d -> %d tile%s", target_label(ed->color_target), c.r, c.g, c.b, changed, changed == 1 ? "" : "s");
    profile_mark_dirty(app, app->profile_editor.last_change);
    ed->synced_hash = selected_tile_hash();
    ed->synced_target = ed->color_target;
    return changed;
}

static uint32_t get_z_value(meta_t *m, int field){
    if (m == NULL) return 0;
    switch (field){
        case 0: return m->bg_for_z;
        case 1: return m->bg_back_z;
        case 2: return m->win_z;
        case 3: return m->obj_z;
        case 4: return m->obj_behind_z;
        default: return m->bg_for_z;
    }
}

static void profile_set_z_for_hash(app_state *app, uint32_t hash, int field, uint32_t value){
    uint32_t z = value;
    if (z >= Z_LAYERS) z = Z_LAYERS - 1;

    if (field == 5){
        set_meta(&app->meta, hash, NULL, NULL, NULL, &z, &z, &z, &z, &z);
        return;
    }

    switch (field){
        case 0:
            set_meta(&app->meta, hash, NULL, NULL, NULL, &z, NULL, NULL, NULL, NULL);
            break;
        case 1:
            set_meta(&app->meta, hash, NULL, NULL, NULL, NULL, &z, NULL, NULL, NULL);
            break;
        case 2:
            set_meta(&app->meta, hash, NULL, NULL, NULL, NULL, NULL, &z, NULL, NULL);
            break;
        case 3:
            set_meta(&app->meta, hash, NULL, NULL, NULL, NULL, NULL, NULL, &z, NULL);
            break;
        case 4:
            set_meta(&app->meta, hash, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &z);
            break;
    }
}

static const char *z_field_label(int field){
    switch (field){
        case 0: return "bg_for_z";
        case 1: return "bg_back_z";
        case 2: return "win_z";
        case 3: return "obj_z";
        case 4: return "obj_behind_z";
        case 5: return "all Z";
        default: return "Z";
    }
}

static int profile_set_z(app_state *app, int field, uint32_t value){
    profile_editor_t *ed = &app->profile_editor;
    uint32_t z = value;
    if (z >= Z_LAYERS) z = Z_LAYERS - 1;

    profile_push_undo(app, "Z change");
    int changed = 0;
    if (ed->selected_count > 0){
        for (int i=0; i<VRAM_TILE_COUNT; i++){
            if (!ed->selected_tiles[i]) continue;
            profile_set_z_for_hash(app, tiles_on_vram[i].hash, field, z);
            changed++;
        }
    }
    else{
        profile_set_z_for_hash(app, selected_tile_hash(), field, z);
        changed = 1;
    }

    profile_set_status(app, "%s set to %u on %d tile%s", z_field_label(field), z, changed, changed == 1 ? "" : "s");
    profile_set_last_change(app, "%s -> %u on %d tile%s", z_field_label(field), z, changed, changed == 1 ? "" : "s");
    profile_mark_dirty(app, app->profile_editor.last_change);
    return changed;
}

static void profile_adjust_z(app_state *app, int field, int delta){
    meta_t *m = get_meta(app->meta, selected_tile_hash());
    uint32_t current = 0;
    if (field == 5) current = m ? m->bg_for_z : 0;
    else current = get_z_value(m, field);

    int next = (int)current + delta;
    next = clamp_int(next, 0, Z_LAYERS - 1);
    profile_set_z(app, field, (uint32_t)next);
}

static int profile_create_meta_for_selection(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    profile_push_undo(app, "create meta");
    int created = 0;
    if (ed->selected_count > 0){
        for (int i=0; i<VRAM_TILE_COUNT; i++){
            if (!ed->selected_tiles[i]) continue;
            ensure_meta_for_hash(app, tiles_on_vram[i].hash);
            created++;
        }
    }
    else{
        ensure_selected_meta(app);
        created = 1;
    }
    profile_set_status(app, "meta ensured for %d tile%s", created, created == 1 ? "" : "s");
    profile_set_last_change(app, "created/kept meta for %d tile%s", created, created == 1 ? "" : "s");
    profile_mark_dirty(app, app->profile_editor.last_change);
    return created;
}

static void profile_set_flag_for_selection(app_state *app, uint32_t flag, bool enabled){
    profile_editor_t *ed = &app->profile_editor;
    profile_push_undo(app, "flag change");
    int changed = 0;
    if (ed->selected_count > 0){
        for (int i=0; i<VRAM_TILE_COUNT; i++){
            if (!ed->selected_tiles[i]) continue;
            meta_t *m = ensure_meta_for_hash(app, tiles_on_vram[i].hash);
            if (m == NULL) continue;
            if (enabled) meta_add_flags(m, flag);
            else meta_clear_flags(m, flag);
            changed++;
        }
    }
    else{
        meta_t *m = ensure_selected_meta(app);
        if (m != NULL){
            if (enabled) meta_add_flags(m, flag);
            else meta_clear_flags(m, flag);
            changed = 1;
        }
    }

    profile_set_status(app, "DRAW_OBJ_C0 %s on %d tile%s", enabled ? "enabled" : "disabled", changed, changed == 1 ? "" : "s");
    profile_set_last_change(app, "DRAW_OBJ_C0 %s -> %d tile%s", enabled ? "ON" : "OFF", changed, changed == 1 ? "" : "s");
    profile_mark_dirty(app, app->profile_editor.last_change);
}

static bool profile_mkdir(const char *path){
    if (path == NULL || path[0] == '\0') return false;
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    return result == 0 || errno == EEXIST;
}

static void profile_join_path(char *out, size_t out_size, const char *a, const char *b){
    if (a == NULL || a[0] == '\0') snprintf(out, out_size, "%s", b ? b : "");
    else if (b == NULL || b[0] == '\0') snprintf(out, out_size, "%s", a);
    else snprintf(out, out_size, "%s/%s", a, b);
}

static bool profile_write_binary_meta_file(const char *path, meta_t *meta){
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;

    char version_buf[10] = {0};
    strncpy(version_buf, VERSION, sizeof(version_buf));
    fwrite(version_buf, 1, sizeof(version_buf), f);

    uint32_t meta_q = 0;
    for (meta_t *m = meta; m != NULL; m = m->next) meta_q++;
    fwrite(&meta_q, sizeof(uint32_t), 1, f);

    for (meta_t *m = meta; m != NULL; m = m->next){
        fwrite(&m->tile_hash, sizeof(uint32_t), 1, f);
        fwrite(&m->bg_color, sizeof(Color), 1, f);
        fwrite(&m->win_color, sizeof(Color), 1, f);
        fwrite(&m->obj_color, sizeof(Color), 1, f);
        fwrite(&m->bg_for_z, sizeof(uint32_t), 1, f);
        fwrite(&m->bg_back_z, sizeof(uint32_t), 1, f);
        fwrite(&m->win_z, sizeof(uint32_t), 1, f);
        fwrite(&m->obj_z, sizeof(uint32_t), 1, f);
        fwrite(&m->obj_behind_z, sizeof(uint32_t), 1, f);
        fwrite(&m->flags, sizeof(uint32_t), 1, f);
    }

    fclose(f);
    return true;
}

static void profile_write_color_json(FILE *f, const char *name, Color c, bool comma){
    fprintf(f, "    \"%s\": [%u, %u, %u, %u]%s\n", name, c.r, c.g, c.b, c.a, comma ? "," : "");
}

static bool profile_write_profile_json(const char *path, meta_t *meta){
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": \"%s\",\n", VERSION);
    fprintf(f, "  \"entries\": [\n");
    for (meta_t *m = meta; m != NULL; m = m->next){
        fprintf(f, "  {\n");
        fprintf(f, "    \"tile_hash\": %u,\n", m->tile_hash);
        profile_write_color_json(f, "bg_color", m->bg_color, true);
        profile_write_color_json(f, "win_color", m->win_color, true);
        profile_write_color_json(f, "obj_color", m->obj_color, true);
        fprintf(f, "    \"bg_for_z\": %u,\n", m->bg_for_z);
        fprintf(f, "    \"bg_back_z\": %u,\n", m->bg_back_z);
        fprintf(f, "    \"win_z\": %u,\n", m->win_z);
        fprintf(f, "    \"obj_z\": %u,\n", m->obj_z);
        fprintf(f, "    \"obj_behind_z\": %u,\n", m->obj_behind_z);
        fprintf(f, "    \"flags\": %u\n", m->flags);
        fprintf(f, "  }%s\n", m->next ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

static Color profile_tile_pixel(tile_t *tile, int col, int row, Color tint){
    if (tile == NULL || tile->raw_data == NULL) return BLANK;
    if (col < 0 || col >= 8 || row < 0 || row >= 8) return BLANK;
    uint8_t lsb = tile->raw_data[row * 2];
    uint8_t msb = tile->raw_data[row * 2 + 1];
    int bit = 7 - col;
    int color_index = ((lsb >> bit) & 1) | (((msb >> bit) & 1) << 1);
    float level = intensity_levels[color_index];
    return (Color){
        (unsigned char)(tint.r * level),
        (unsigned char)(tint.g * level),
        (unsigned char)(tint.b * level),
        255
    };
}

static void profile_image_set(Color *pixels, int width, int height, int x, int y, Color c){
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    pixels[y * width + x] = c;
}

static void profile_image_rect_lines(Color *pixels, int width, int height, int x, int y, int w, int h, Color c){
    for (int i=0; i<w; i++){
        profile_image_set(pixels, width, height, x + i, y, c);
        profile_image_set(pixels, width, height, x + i, y + h - 1, c);
    }
    for (int i=0; i<h; i++){
        profile_image_set(pixels, width, height, x, y + i, c);
        profile_image_set(pixels, width, height, x + w - 1, y + i, c);
    }
}

static bool profile_export_image_file(const char *path, Color *pixels, int width, int height){
    Image img = (Image){ pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    return ExportImage(img, path);
}

static bool profile_export_frame_image(app_state *app, const char *path, int scale){
    if (scale < 1) scale = 1;
    int width = LCD_WIDTH * scale;
    int height = LCD_HEIGHT * scale;
    Color *pixels = calloc((size_t)width * (size_t)height, sizeof(Color));
    if (pixels == NULL) return false;

    framebuffer_t *fb = &app->framebuffers[0];
    if (fb->copy != NULL) fb = fb->copy;

    for (int y=0; y<LCD_HEIGHT; y++){
        for (int x=0; x<LCD_WIDTH; x++){
            Color c = BLANK;
            memcpy(&c, &fb->pixels[y][x], sizeof(Color));
            if (c.a == 0) c = BG_COLOR;
            for (int yy=0; yy<scale; yy++){
                for (int xx=0; xx<scale; xx++){
                    profile_image_set(pixels, width, height, x * scale + xx, y * scale + yy, c);
                }
            }
        }
    }

    bool ok = profile_export_image_file(path, pixels, width, height);
    free(pixels);
    return ok;
}


static void profile_write_color_value(FILE *f, Color c){
    fprintf(f, "[%u, %u, %u, %u]", c.r, c.g, c.b, c.a);
}

static bool profile_color_equal(Color a, Color b){
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool profile_export_tile_grid_image(app_state *app, const char *path, bool selected_only, bool labeled){
    profile_editor_t *ed = &app->profile_editor;
    int count = selected_only ? profile_effective_selection_count(ed) : VRAM_TILE_COUNT;
    if (count < 1) count = 1;

    int cols = selected_only ? 8 : VRAM_INSPECTOR_WIDTH;
    if (count < cols) cols = count;
    int rows = (count + cols - 1) / cols;
    int scale = selected_only ? 8 : 3;
    int tile_cell = 8 * scale;
    int label_h = labeled ? 34 : 0;
    int cell_w = tile_cell;
    int cell_h = tile_cell + label_h;
    int width = cols * cell_w;
    int height = rows * cell_h;
    Color *pixels = calloc((size_t)width * (size_t)height, sizeof(Color));
    if (pixels == NULL) return false;

    for (int i=0; i<width*height; i++) pixels[i] = (Color){18, 20, 24, 255};

    int selected_indices[VRAM_TILE_COUNT];
    int out_i = 0;
    for (int tile_index=0; tile_index<VRAM_TILE_COUNT; tile_index++){
        if (selected_only && !profile_has_tile_in_selection(ed, tile_index)) continue;
        if (out_i >= VRAM_TILE_COUNT) break;
        selected_indices[out_i] = tile_index;

        int col = out_i % cols;
        int row = out_i / cols;
        int ox = col * cell_w;
        int oy = row * cell_h;
        for (int ty=0; ty<8; ty++){
            for (int tx=0; tx<8; tx++){
                Color c = profile_tile_pixel(&tiles_on_vram[tile_index], tx, ty, WHITE);
                for (int yy=0; yy<scale; yy++){
                    for (int xx=0; xx<scale; xx++){
                        profile_image_set(pixels, width, height, ox + tx * scale + xx, oy + ty * scale + yy, c);
                    }
                }
            }
        }
        if (app->profile_editor.used_tiles[tile_index]){
            profile_image_rect_lines(pixels, width, height, ox + 1, oy + 1, tile_cell - 2, tile_cell - 2, (Color){70, 220, 110, 255});
        }
        if (tile_index == selected_tile){
            profile_image_rect_lines(pixels, width, height, ox + 2, oy + 2, tile_cell - 4, tile_cell - 4, PINK);
        }
        out_i++;
        if (selected_only && out_i >= count) break;
    }

    Image img = (Image){ pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    if (labeled){
        char label[96];
        for (int i=0; i<out_i; i++){
            int tile_index = selected_indices[i];
            int col = i % cols;
            int row = i / cols;
            int ox = col * cell_w;
            int oy = row * cell_h + tile_cell + 3;
            snprintf(label, sizeof(label), "#%d", tile_index);
            ImageDrawText(&img, label, ox + 3, oy, 10, WHITE);
            snprintf(label, sizeof(label), "%u", tiles_on_vram[tile_index].hash);
            ImageDrawText(&img, label, ox + 3, oy + 13, 9, (Color){210, 210, 210, 255});
            ImageDrawText(&img, get_meta(app->meta, tiles_on_vram[tile_index].hash) ? "meta" : "no meta", ox + 3, oy + 24, 8, get_meta(app->meta, tiles_on_vram[tile_index].hash) ? GREEN : GRAY);
        }
    }

    bool ok = ExportImage(img, path);
    free(pixels);
    return ok;
}

static bool profile_write_manifest(app_state *app, const char *path){
    profile_editor_t *ed = &app->profile_editor;
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"paused\": %s,\n", app->paused ? "true" : "false");
    fprintf(f, "  \"current_tile\": %d,\n", selected_tile);
    fprintf(f, "  \"current_hash\": %u,\n", selected_tile_hash());
    fprintf(f, "  \"tile_count\": %d,\n", VRAM_TILE_COUNT);
    fprintf(f, "  \"color_target\": \"%s\",\n", target_label(ed->color_target));
    fprintf(f, "  \"pending_color\": [%d, %d, %d, 255],\n", ed->color_r, ed->color_g, ed->color_b);
    fprintf(f, "  \"profile_file\": \"%s\",\n", ed->filename);
    fprintf(f, "  \"files\": {\n");
    fprintf(f, "    \"all_tiles_folder\": \"all_tiles\",\n");
    fprintf(f, "    \"tiles_json\": \"all_tiles/tiles.json\",\n");
    fprintf(f, "    \"tiles_csv\": \"all_tiles/tiles.csv\"\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}


static void profile_write_tile_csv_row(FILE *f, app_state *app, int tile_index){
    uint32_t hash = tiles_on_vram[tile_index].hash;
    meta_t *m = get_meta(app->meta, hash);
    if (m != NULL){
        fprintf(f, "%d,%u,%s,true,%u/%u/%u/%u,%u/%u/%u/%u,%u/%u/%u/%u,%u,%u,%u,%u,%u,%u,",
            tile_index, hash, app->profile_editor.used_tiles[tile_index] ? "true" : "false",
            m->bg_color.r, m->bg_color.g, m->bg_color.b, m->bg_color.a,
            m->win_color.r, m->win_color.g, m->win_color.b, m->win_color.a,
            m->obj_color.r, m->obj_color.g, m->obj_color.b, m->obj_color.a,
            m->bg_for_z, m->bg_back_z, m->win_z, m->obj_z, m->obj_behind_z, m->flags);
    }
    else{
        fprintf(f, "%d,%u,%s,false,,,,,,,,,,", tile_index, hash, app->profile_editor.used_tiles[tile_index] ? "true" : "false");
    }

    if (tiles_on_vram[tile_index].raw_data != NULL){
        for (int i=0; i<TILE_SIZE; i++) fprintf(f, "%02X", tiles_on_vram[tile_index].raw_data[i]);
    }
    fprintf(f, "\n");
}

static bool profile_write_all_tiles_csv(app_state *app, const char *path){
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;
    fprintf(f, "index,hash,used_on_frame,meta_present,bg_color,win_color,obj_color,bg_for_z,bg_back_z,win_z,obj_z,obj_behind_z,flags,raw_tile_hex\n");
    for (int i=0; i<VRAM_TILE_COUNT; i++) profile_write_tile_csv_row(f, app, i);
    fclose(f);
    return true;
}

static void profile_write_tile_json_object(FILE *f, app_state *app, int tile_index, bool comma){
    uint32_t hash = tiles_on_vram[tile_index].hash;
    meta_t *m = get_meta(app->meta, hash);
    fprintf(f, "    {\n");
    fprintf(f, "      \"index\": %d,\n", tile_index);
    fprintf(f, "      \"hash\": %u,\n", hash);
    fprintf(f, "      \"used_on_frame\": %s,\n", app->profile_editor.used_tiles[tile_index] ? "true" : "false");
    fprintf(f, "      \"meta_present\": %s,\n", m ? "true" : "false");
    fprintf(f, "      \"raw_tile_hex\": \"");
    if (tiles_on_vram[tile_index].raw_data != NULL){
        for (int i=0; i<TILE_SIZE; i++) fprintf(f, "%02X", tiles_on_vram[tile_index].raw_data[i]);
    }
    fprintf(f, "\"");
    if (m != NULL){
        fprintf(f, ",\n      \"bg_color\": [%u, %u, %u, %u]", m->bg_color.r, m->bg_color.g, m->bg_color.b, m->bg_color.a);
        fprintf(f, ",\n      \"win_color\": [%u, %u, %u, %u]", m->win_color.r, m->win_color.g, m->win_color.b, m->win_color.a);
        fprintf(f, ",\n      \"obj_color\": [%u, %u, %u, %u]", m->obj_color.r, m->obj_color.g, m->obj_color.b, m->obj_color.a);
        fprintf(f, ",\n      \"bg_for_z\": %u", m->bg_for_z);
        fprintf(f, ",\n      \"bg_back_z\": %u", m->bg_back_z);
        fprintf(f, ",\n      \"win_z\": %u", m->win_z);
        fprintf(f, ",\n      \"obj_z\": %u", m->obj_z);
        fprintf(f, ",\n      \"obj_behind_z\": %u", m->obj_behind_z);
        fprintf(f, ",\n      \"flags\": %u", m->flags);
    }
    fprintf(f, "\n    }%s\n", comma ? "," : "");
}

static bool profile_write_all_tiles_json(app_state *app, const char *path){
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"tile_count\": %d,\n", VRAM_TILE_COUNT);
    fprintf(f, "  \"current_tile\": %d,\n", selected_tile);
    fprintf(f, "  \"tiles\": [\n");
    for (int i=0; i<VRAM_TILE_COUNT; i++) profile_write_tile_json_object(f, app, i, i < VRAM_TILE_COUNT - 1);
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

static bool profile_export_single_tile_image(app_state *app, const char *path, int tile_index, int scale, bool labeled){
    if (tile_index < 0 || tile_index >= VRAM_TILE_COUNT) return false;
    if (scale < 1) scale = 1;

    int tile_size = 8 * scale;
    int label_h = labeled ? 42 : 0;
    int pad = 8;
    int width = tile_size + pad * 2;
    int height = tile_size + pad * 2 + label_h;
    Color *pixels = calloc((size_t)width * (size_t)height, sizeof(Color));
    if (pixels == NULL) return false;
    for (int i=0; i<width*height; i++) pixels[i] = (Color){18, 20, 24, 255};

    for (int ty=0; ty<8; ty++){
        for (int tx=0; tx<8; tx++){
            Color c = profile_tile_pixel(&tiles_on_vram[tile_index], tx, ty, WHITE);
            for (int yy=0; yy<scale; yy++){
                for (int xx=0; xx<scale; xx++){
                    profile_image_set(pixels, width, height, pad + tx * scale + xx, pad + ty * scale + yy, c);
                }
            }
        }
    }

    profile_image_rect_lines(pixels, width, height, pad, pad, tile_size, tile_size, WHITE);
    if (app->profile_editor.used_tiles[tile_index]){
        profile_image_rect_lines(pixels, width, height, pad + 1, pad + 1, tile_size - 2, tile_size - 2, (Color){70, 220, 110, 255});
    }
    if (tile_index == selected_tile){
        profile_image_rect_lines(pixels, width, height, pad + 2, pad + 2, tile_size - 4, tile_size - 4, PINK);
    }

    Image img = (Image){ pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    if (labeled){
        char label[128];
        int y = pad + tile_size + 5;
        snprintf(label, sizeof(label), "tile %03d", tile_index);
        ImageDrawText(&img, label, pad, y, 12, WHITE);
        snprintf(label, sizeof(label), "hash %u", tiles_on_vram[tile_index].hash);
        ImageDrawText(&img, label, pad, y + 14, 10, (Color){210, 210, 210, 255});
        ImageDrawText(&img, get_meta(app->meta, tiles_on_vram[tile_index].hash) ? "meta" : "no meta", pad, y + 27, 10, get_meta(app->meta, tiles_on_vram[tile_index].hash) ? GREEN : GRAY);
    }

    bool ok = ExportImage(img, path);
    free(pixels);
    return ok;
}

static bool profile_write_all_tiles_summary(app_state *app, const char *path){
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;
    int meta_tiles = 0;
    for (int i=0; i<VRAM_TILE_COUNT; i++){
        if (get_meta(app->meta, tiles_on_vram[i].hash) != NULL) meta_tiles++;
    }

    fprintf(f, "3DGB Full Tile Export\n");
    fprintf(f, "=====================\n\n");
    fprintf(f, "Tile count      : %d\n", VRAM_TILE_COUNT);
    fprintf(f, "Tiles with meta : %d\n", meta_tiles);
    fprintf(f, "Current tile    : %d\n", selected_tile);
    fprintf(f, "Current hash    : %u\n\n", selected_tile_hash());
    fprintf(f, "Files in this folder:\n");
    fprintf(f, "- tiles.csv: table for every VRAM tile, including raw tile bytes and current-frame usage.\n");
    fprintf(f, "- tiles.json: structured data for every VRAM tile.\n");
    fprintf(f, "- all_tiles.png: complete VRAM tile grid.\n");
    fprintf(f, "- all_tiles_labeled.png: complete VRAM tile grid with labels.\n");
    fprintf(f, "- tile_XXX_hash_YYYY.png: one image per VRAM tile.\n");
    fclose(f);
    return true;
}

static bool profile_export_all_tiles_folder(app_state *app, const char *session_dir){
    char dir[512];
    profile_join_path(dir, sizeof(dir), session_dir, "all_tiles");
    if (!profile_mkdir(dir)) return false;

    char path[768];
    profile_join_path(path, sizeof(path), dir, "all_tiles.png");
    profile_export_tile_grid_image(app, path, false, false);
    profile_join_path(path, sizeof(path), dir, "all_tiles_labeled.png");
    profile_export_tile_grid_image(app, path, false, true);
    profile_join_path(path, sizeof(path), dir, "tiles.csv");
    profile_write_all_tiles_csv(app, path);
    profile_join_path(path, sizeof(path), dir, "tiles.json");
    profile_write_all_tiles_json(app, path);
    profile_join_path(path, sizeof(path), dir, "README.txt");
    profile_write_all_tiles_summary(app, path);

    for (int i=0; i<VRAM_TILE_COUNT; i++){
        char filename[96];
        snprintf(filename, sizeof(filename), "tile_%03d_hash_%u.png", i, tiles_on_vram[i].hash);
        profile_join_path(path, sizeof(path), dir, filename);
        profile_export_single_tile_image(app, path, i, 8, true);
    }
    return true;
}

static bool profile_write_export_summary(app_state *app, const char *path){
    profile_editor_t *ed = &app->profile_editor;
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;
    fprintf(f, "3DGB Profile Exchange Package\n");
    fprintf(f, "=============================\n\n");
    fprintf(f, "Emulation state : %s\n", app->paused ? "Paused" : "Running");
    fprintf(f, "Current tile    : %d\n", selected_tile);
    fprintf(f, "Current hash    : %u\n", selected_tile_hash());
    fprintf(f, "Color target    : %s\n", target_label(ed->color_target));
    fprintf(f, "Pending color   : %d %d %d 255\n", ed->color_r, ed->color_g, ed->color_b);
    fprintf(f, "Profile file    : %s\n\n", ed->filename);
    fprintf(f, "Files:\n");
    fprintf(f, "- window_snapshot.png: full application window at export time.\n");
    fprintf(f, "- frame_full.png: emulator frame only.\n");
    fprintf(f, "- tile_grid.png: current VRAM tile grid.\n");
    fprintf(f, "- all_tiles/: images and data for every VRAM tile.\n");
    fprintf(f, "- manifest.json: package metadata.\n");
    fprintf(f, "- profile_current.json / profile_current.meta: current profile snapshot.\n");
    fclose(f);
    return true;
}

static bool profile_export_package(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char session[64];
    if (tm_now != NULL) strftime(session, sizeof(session), "session_%Y%m%d_%H%M%S", tm_now);
    else snprintf(session, sizeof(session), "session_export");

    if (!profile_mkdir(ed->export_dir)){
        profile_set_status(app, "could not create export directory");
        return false;
    }

    char dir[256];
    profile_join_path(dir, sizeof(dir), ed->export_dir, session);
    if (!profile_mkdir(dir)){
        profile_set_status(app, "could not create export package directory");
        return false;
    }

    char path[512];
    profile_join_path(path, sizeof(path), dir, "window_snapshot.png");
    TakeScreenshot(path);
    profile_join_path(path, sizeof(path), dir, "frame_full.png");
    profile_export_frame_image(app, path, 3);
    profile_join_path(path, sizeof(path), dir, "tile_grid.png");
    profile_export_tile_grid_image(app, path, false, false);
    profile_join_path(path, sizeof(path), dir, "profile_current.json");
    profile_write_profile_json(path, app->meta);
    profile_join_path(path, sizeof(path), dir, "profile_current.meta");
    profile_write_binary_meta_file(path, app->meta);
    profile_join_path(path, sizeof(path), dir, "manifest.json");
    profile_write_manifest(app, path);
    profile_export_all_tiles_folder(app, dir);
    profile_join_path(path, sizeof(path), dir, "README.txt");
    profile_write_export_summary(app, path);

    snprintf(ed->last_export_path, sizeof(ed->last_export_path), "%s", dir);
    profile_set_status(app, "exported all-tile package: %s", dir);
    profile_set_last_change(app, "exported all-tile package -> %s", dir);
    return true;
}

static char *profile_read_text_file(const char *path){
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > 8 * 1024 * 1024){ fclose(f); return NULL; }
    char *text = malloc((size_t)size + 1);
    if (text == NULL){ fclose(f); return NULL; }
    if (fread(text, 1, (size_t)size, f) != (size_t)size){ free(text); fclose(f); return NULL; }
    text[size] = '\0';
    fclose(f);
    return text;
}

static const char *json_find_value(const char *obj, const char *key){
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(obj, pattern);
    if (p == NULL) return NULL;
    p = strchr(p, ':');
    if (p == NULL) return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static bool json_parse_uint_field(const char *obj, const char *key, uint32_t *out){
    const char *p = json_find_value(obj, key);
    if (p == NULL) return false;
    char *end = NULL;
    unsigned long v = strtoul(p, &end, 10);
    if (end == p) return false;
    *out = (uint32_t)v;
    return true;
}

static bool json_parse_uint_any(const char *obj, const char *a, const char *b, uint32_t *out){
    return json_parse_uint_field(obj, a, out) || json_parse_uint_field(obj, b, out);
}

static bool parse_rgb_numbers(const char *p, int *r, int *g, int *b, int *a){
    int rr=0, gg=0, bb=0, aa=255;
    int count = sscanf(p, " %d , %d , %d , %d", &rr, &gg, &bb, &aa);
    if (count < 3) return false;
    *r = rr;
    *g = gg;
    *b = bb;
    *a = aa;
    return true;
}

static bool json_parse_color_field(const char *obj, const char *key, Color *out){
    const char *p = json_find_value(obj, key);
    if (p == NULL) return false;

    int r=0, g=0, b=0, a=255;
    if (*p == '['){
        if (!parse_rgb_numbers(p + 1, &r, &g, &b, &a)) return false;
    }
    else if (*p == '"'){
        if (!parse_rgb_numbers(p + 1, &r, &g, &b, &a)) return false;
    }
    else{
        if (!parse_rgb_numbers(p, &r, &g, &b, &a)) return false;
    }

    *out = (Color){
        (unsigned char)clamp_int(r, 0, 255),
        (unsigned char)clamp_int(g, 0, 255),
        (unsigned char)clamp_int(b, 0, 255),
        (unsigned char)clamp_int(a, 0, 255)
    };
    return true;
}

static bool json_parse_color_any(const char *obj, const char *a, const char *b, Color *out){
    return json_parse_color_field(obj, a, out) || json_parse_color_field(obj, b, out);
}

static int profile_parse_import_json(profile_editor_t *ed, const char *json){
    ed->import_entry_count = 0;
    ed->import_loaded = false;

    const char *p = json;
    while ((p = strchr(p, '{')) != NULL && ed->import_entry_count < PROFILE_IMPORT_MAX_ENTRIES){
        const char *end = strchr(p, '}');
        if (end == NULL) break;
        size_t len = (size_t)(end - p + 1);
        if (len > 8191) len = 8191;

        char obj[8192];
        memcpy(obj, p, len);
        obj[len] = '\0';

        profile_import_entry_t *entry = &ed->import_entries[ed->import_entry_count];
        memset(entry, 0, sizeof(*entry));
        if (json_parse_uint_any(obj, "tile_hash", "hash", &entry->tile_hash)){
            entry->has_bg_color = json_parse_color_any(obj, "bg_color", "bg_color_rgb", &entry->bg_color);
            entry->has_win_color = json_parse_color_any(obj, "win_color", "win_color_rgb", &entry->win_color);
            entry->has_obj_color = json_parse_color_any(obj, "obj_color", "obj_color_rgb", &entry->obj_color);
            entry->has_bg_for_z = json_parse_uint_field(obj, "bg_for_z", &entry->bg_for_z);
            entry->has_bg_back_z = json_parse_uint_field(obj, "bg_back_z", &entry->bg_back_z);
            entry->has_win_z = json_parse_uint_field(obj, "win_z", &entry->win_z);
            entry->has_obj_z = json_parse_uint_field(obj, "obj_z", &entry->obj_z);
            entry->has_obj_behind_z = json_parse_uint_field(obj, "obj_behind_z", &entry->obj_behind_z);
            entry->has_flags = json_parse_uint_field(obj, "flags", &entry->flags);
            ed->import_entry_count++;
        }
        p = end + 1;
    }

    ed->import_loaded = ed->import_entry_count > 0;
    return ed->import_entry_count;
}


static int profile_count_import_entry_fields(profile_import_entry_t *e){
    int count = 0;
    if (e->has_bg_color) count++;
    if (e->has_win_color) count++;
    if (e->has_obj_color) count++;
    if (e->has_bg_for_z) count++;
    if (e->has_bg_back_z) count++;
    if (e->has_win_z) count++;
    if (e->has_obj_z) count++;
    if (e->has_obj_behind_z) count++;
    if (e->has_flags) count++;
    return count;
}

static int profile_count_import_entry_changes(meta_t *m, profile_import_entry_t *e){
    if (m == NULL) return profile_count_import_entry_fields(e);
    int changes = 0;
    if (e->has_bg_color && !profile_color_equal(m->bg_color, e->bg_color)) changes++;
    if (e->has_win_color && !profile_color_equal(m->win_color, e->win_color)) changes++;
    if (e->has_obj_color && !profile_color_equal(m->obj_color, e->obj_color)) changes++;
    if (e->has_bg_for_z && m->bg_for_z != e->bg_for_z) changes++;
    if (e->has_bg_back_z && m->bg_back_z != e->bg_back_z) changes++;
    if (e->has_win_z && m->win_z != e->win_z) changes++;
    if (e->has_obj_z && m->obj_z != e->obj_z) changes++;
    if (e->has_obj_behind_z && m->obj_behind_z != e->obj_behind_z) changes++;
    if (e->has_flags && m->flags != e->flags) changes++;
    return changes;
}

static void profile_write_optional_color_change(FILE *f, const char *name, bool present, meta_t *m, Color before, Color after, int *written){
    if (!present) return;
    if ((*written)++ > 0) fprintf(f, ",\n");
    fprintf(f, "        \"%s\": {\"before\": ", name);
    if (m == NULL) fprintf(f, "null");
    else profile_write_color_value(f, before);
    fprintf(f, ", \"after\": ");
    profile_write_color_value(f, after);
    fprintf(f, "}");
}

static void profile_write_optional_uint_change(FILE *f, const char *name, bool present, meta_t *m, uint32_t before, uint32_t after, int *written){
    if (!present) return;
    if ((*written)++ > 0) fprintf(f, ",\n");
    fprintf(f, "        \"%s\": {\"before\": ", name);
    if (m == NULL) fprintf(f, "null");
    else fprintf(f, "%u", before);
    fprintf(f, ", \"after\": %u}", after);
}

static bool profile_write_import_preview_json(app_state *app, const char *path, const char *source_path){
    profile_editor_t *ed = &app->profile_editor;
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;

    ed->import_preview_existing = 0;
    ed->import_preview_new = 0;
    ed->import_preview_changed_entries = 0;
    ed->import_preview_field_changes = 0;
    snprintf(ed->import_preview_path, sizeof(ed->import_preview_path), "%s", path);

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"source\": \"%s\",\n", source_path ? source_path : "");
    fprintf(f, "  \"entries_loaded\": %d,\n", ed->import_entry_count);
    fprintf(f, "  \"entries\": [\n");

    for (int i=0; i<ed->import_entry_count; i++){
        profile_import_entry_t *e = &ed->import_entries[i];
        meta_t *m = get_meta(app->meta, e->tile_hash);
        int changes = profile_count_import_entry_changes(m, e);
        if (m == NULL) ed->import_preview_new++;
        else ed->import_preview_existing++;
        if (changes > 0) ed->import_preview_changed_entries++;
        ed->import_preview_field_changes += changes;

        fprintf(f, "    {\n");
        fprintf(f, "      \"tile_hash\": %u,\n", e->tile_hash);
        fprintf(f, "      \"status\": \"%s\",\n", m == NULL ? "new" : (changes > 0 ? "changed" : "unchanged"));
        fprintf(f, "      \"changed_fields\": %d,\n", changes);
        fprintf(f, "      \"changes\": {\n");

        int written = 0;
        profile_write_optional_color_change(f, "bg_color", e->has_bg_color && (m == NULL || !profile_color_equal(m->bg_color, e->bg_color)), m, m ? m->bg_color : BLACK, e->bg_color, &written);
        profile_write_optional_color_change(f, "win_color", e->has_win_color && (m == NULL || !profile_color_equal(m->win_color, e->win_color)), m, m ? m->win_color : BLACK, e->win_color, &written);
        profile_write_optional_color_change(f, "obj_color", e->has_obj_color && (m == NULL || !profile_color_equal(m->obj_color, e->obj_color)), m, m ? m->obj_color : BLACK, e->obj_color, &written);
        profile_write_optional_uint_change(f, "bg_for_z", e->has_bg_for_z && (m == NULL || m->bg_for_z != e->bg_for_z), m, m ? m->bg_for_z : 0, e->bg_for_z, &written);
        profile_write_optional_uint_change(f, "bg_back_z", e->has_bg_back_z && (m == NULL || m->bg_back_z != e->bg_back_z), m, m ? m->bg_back_z : 0, e->bg_back_z, &written);
        profile_write_optional_uint_change(f, "win_z", e->has_win_z && (m == NULL || m->win_z != e->win_z), m, m ? m->win_z : 0, e->win_z, &written);
        profile_write_optional_uint_change(f, "obj_z", e->has_obj_z && (m == NULL || m->obj_z != e->obj_z), m, m ? m->obj_z : 0, e->obj_z, &written);
        profile_write_optional_uint_change(f, "obj_behind_z", e->has_obj_behind_z && (m == NULL || m->obj_behind_z != e->obj_behind_z), m, m ? m->obj_behind_z : 0, e->obj_behind_z, &written);
        profile_write_optional_uint_change(f, "flags", e->has_flags && (m == NULL || m->flags != e->flags), m, m ? m->flags : 0, e->flags, &written);
        if (written > 0) fprintf(f, "\n");
        fprintf(f, "      }\n");
        fprintf(f, "    }%s\n", (i + 1 < ed->import_entry_count) ? "," : "");
    }

    fprintf(f, "  ],\n");
    fprintf(f, "  \"summary\": {\n");
    fprintf(f, "    \"existing_entries\": %d,\n", ed->import_preview_existing);
    fprintf(f, "    \"new_entries\": %d,\n", ed->import_preview_new);
    fprintf(f, "    \"changed_entries\": %d,\n", ed->import_preview_changed_entries);
    fprintf(f, "    \"field_changes\": %d\n", ed->import_preview_field_changes);
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

static int profile_load_import_entries(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    char import_dir[64] = "imports";
    char path[512];
    char *json = NULL;

    profile_join_path(path, sizeof(path), import_dir, ed->exchange_filename);
    json = profile_read_text_file(path);
    if (json == NULL){
        snprintf(path, sizeof(path), "%s", ed->exchange_filename);
        json = profile_read_text_file(path);
    }

    if (json == NULL){
        profile_set_status(app, "import file not found: imports/%s", ed->exchange_filename);
        ed->import_loaded = false;
        ed->import_entry_count = 0;
        return 0;
    }

    int count = profile_parse_import_json(ed, json);
    free(json);

    profile_mkdir(import_dir);
    char preview_path[512];
    profile_join_path(preview_path, sizeof(preview_path), import_dir, "import_preview.json");
    profile_write_import_preview_json(app, preview_path, path);

    profile_set_status(app, "preview: %d entries, %d changed, %d new, %d field changes", count, ed->import_preview_changed_entries, ed->import_preview_new, ed->import_preview_field_changes);
    profile_set_last_change(app, "preview import -> %d entries / %d field changes", count, ed->import_preview_field_changes);
    return count;
}

static int profile_apply_import_entries(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    if (!ed->import_loaded && profile_load_import_entries(app) <= 0) return 0;

    profile_push_undo(app, "import apply");
    int applied = 0;
    for (int i=0; i<ed->import_entry_count; i++){
        profile_import_entry_t *e = &ed->import_entries[i];
        Color *bg = e->has_bg_color ? &e->bg_color : NULL;
        Color *win = e->has_win_color ? &e->win_color : NULL;
        Color *obj = e->has_obj_color ? &e->obj_color : NULL;
        uint32_t *bg_for_z = e->has_bg_for_z ? &e->bg_for_z : NULL;
        uint32_t *bg_back_z = e->has_bg_back_z ? &e->bg_back_z : NULL;
        uint32_t *win_z = e->has_win_z ? &e->win_z : NULL;
        uint32_t *obj_z = e->has_obj_z ? &e->obj_z : NULL;
        uint32_t *obj_behind_z = e->has_obj_behind_z ? &e->obj_behind_z : NULL;

        set_meta(&app->meta, e->tile_hash, bg, win, obj, bg_for_z, bg_back_z, win_z, obj_z, obj_behind_z);
        if (e->has_flags){
            meta_t *m = ensure_meta_for_hash(app, e->tile_hash);
            if (m != NULL) meta_set_flags(m, e->flags);
        }
        applied++;
    }

    ed->synced_hash = 0;
    profile_set_status(app, "applied %d imported entr%s", applied, applied == 1 ? "y" : "ies");
    profile_set_last_change(app, "applied import -> %d entr%s", applied, applied == 1 ? "y" : "ies");
    profile_mark_dirty(app, app->profile_editor.last_change);
    return applied;
}

static char ui_tooltip[256];

static void ui_tooltip_reset(void){
    ui_tooltip[0] = '\0';
}

static void ui_set_tooltip(const char *text){
    if (text == NULL || text[0] == '\0') return;
    strncpy(ui_tooltip, text, sizeof(ui_tooltip) - 1);
    ui_tooltip[sizeof(ui_tooltip) - 1] = '\0';
}

static void ui_tooltip_draw(void){
    if (ui_tooltip[0] == '\0') return;
    Vector2 mouse = GetMousePosition();
    int width = MeasureText(ui_tooltip, 12) + 14;
    int x = (int)mouse.x + 14;
    int y = (int)mouse.y + 16;
    if (x + width > GetScreenWidth()) x = GetScreenWidth() - width - 8;
    if (y + 26 > GetScreenHeight()) y = GetScreenHeight() - 28;
    DrawRectangle(x, y, width, 24, (Color){18, 20, 24, 235});
    DrawRectangleLines(x, y, width, 24, (Color){160, 160, 160, 255});
    DrawText(ui_tooltip, x + 7, y + 6, 12, WHITE);
}

static bool ui_button_ex(Rectangle r, const char *text, const char *tooltip){
    Vector2 mouse = GetMousePosition();
    bool hot = CheckCollisionPointRec(mouse, r);
    Color bg = hot ? (Color){90, 96, 105, 255} : (Color){58, 62, 70, 255};
    DrawRectangleRec(r, bg);
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){180, 180, 180, 255});
    DrawText(text, (int)r.x + 8, (int)r.y + 6, 16, WHITE);
    if (hot) ui_set_tooltip(tooltip);
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static bool ui_button(Rectangle r, const char *text){
    return ui_button_ex(r, text, NULL);
}

static bool ui_button_tip(Rectangle r, const char *text, const char *tooltip){
    return ui_button_ex(r, text, tooltip);
}

static bool ui_small_button(int x, int y, const char *text){
    return ui_button((Rectangle){x, y, 28, 24}, text);
}

static bool ui_small_button_tip(int x, int y, const char *text, const char *tooltip){
    return ui_button_tip((Rectangle){x, y, 28, 24}, text, tooltip);
}

static bool ui_slider(profile_editor_t *ed, int field_id, int x, int y, const char *label, int *value){
    char value_text[32];
    int old = *value;
    Rectangle bar = (Rectangle){x + 48, y + 8, 190, 10};
    Rectangle value_box = (Rectangle){x + 250, y - 2, 44, 24};
    Vector2 mouse = GetMousePosition();

    DrawText(label, x, y, 18, WHITE);
    DrawRectangleRec(bar, (Color){45, 45, 45, 255});
    DrawRectangleLines((int)bar.x, (int)bar.y, (int)bar.width, (int)bar.height, (Color){160, 160, 160, 255});

    Rectangle hitbox = (Rectangle){bar.x - 4, bar.y - 8, bar.width + 8, 26};
    if (CheckCollisionPointRec(mouse, hitbox)) ui_set_tooltip("Drag to change the pending color value");
    if ((IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) && CheckCollisionPointRec(mouse, hitbox)){
        float ratio = (mouse.x - bar.x) / bar.width;
        *value = clamp_int((int)(ratio * 255.0f + 0.5f), 0, 255);
        if (ed->value_editing_id == field_id) ed->value_editing_id = 0;
    }

    int knob_x = (int)(bar.x + ((*value) / 255.0f) * bar.width);
    DrawRectangle(knob_x - 3, (int)bar.y - 5, 6, 20, WHITE);

    bool hot_value = CheckCollisionPointRec(mouse, value_box);
    if (hot_value) ui_set_tooltip("Click and type a value from 0 to 255");
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hot_value){
        ed->value_editing_id = field_id;
        snprintf(ed->value_edit_text, sizeof(ed->value_edit_text), "%d", *value);
    }

    if (ed->value_editing_id == field_id){
        int len = (int)strlen(ed->value_edit_text);
        int ch = GetCharPressed();
        while (ch > 0){
            if (ch >= '0' && ch <= '9' && len < (int)sizeof(ed->value_edit_text) - 1){
                ed->value_edit_text[len++] = (char)ch;
                ed->value_edit_text[len] = '\0';
            }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && len > 0){
            ed->value_edit_text[len - 1] = '\0';
            len--;
        }
        if (IsKeyPressed(KEY_ENTER)){
            *value = clamp_int(atoi(ed->value_edit_text), 0, 255);
            ed->value_editing_id = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) ed->value_editing_id = 0;
    }

    DrawRectangleRec(value_box, ed->value_editing_id == field_id ? (Color){40, 46, 58, 255} : (Color){35, 35, 35, 255});
    DrawRectangleLines((int)value_box.x, (int)value_box.y, (int)value_box.width, (int)value_box.height, ed->value_editing_id == field_id ? YELLOW : (Color){160, 160, 160, 255});
    if (ed->value_editing_id == field_id) snprintf(value_text, sizeof(value_text), "%s", ed->value_edit_text);
    else snprintf(value_text, sizeof(value_text), "%d", *value);
    DrawText(value_text, (int)value_box.x + 7, (int)value_box.y + 4, 16, WHITE);

    return old != *value;
}

static void profile_update_filename(profile_editor_t *ed){
    if (!ed->filename_editing) return;

    int len = (int)strlen(ed->filename);
    int ch = GetCharPressed();
    while (ch > 0){
        if (ch >= 32 && ch <= 126 && len < (int)sizeof(ed->filename) - 1){
            ed->filename[len++] = (char)ch;
            ed->filename[len] = '\0';
        }
        ch = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && len > 0){
        ed->filename[len - 1] = '\0';
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)){
        ed->filename_editing = false;
    }
}

static void profile_update(app_state *app){
    profile_editor_t *ed = &app->profile_editor;

    if (IsKeyPressed(KEY_F1)){
        ed->open = !ed->open;
    }
    if (app->state_machine != ON_COMMAND_BAR_STATE){
        bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (ctrl_down && IsKeyPressed(KEY_Z)) profile_undo(app);
        if (ctrl_down && IsKeyPressed(KEY_Y)) profile_redo(app);
        if (IsKeyPressed(KEY_F2)){
            app->paused = !app->paused;
            profile_set_status(app, "emulation %s", app->paused ? "paused" : "running");
        }
        if (IsKeyPressed(KEY_F3)){
            app->state_machine = ON_COMMAND_BAR_STATE;
            profile_set_status(app, "command bar opened");
        }
        if (IsKeyPressed(KEY_F6)){
            profile_export_package(app);
        }
        if (IsKeyPressed(KEY_F7)){
            profile_load_import_entries(app);
        }
        if (IsKeyPressed(KEY_F8)){
            profile_apply_import_entries(app);
        }
        if (IsKeyPressed(KEY_F10)){
            ed->panel_page = (ed->panel_page + 1) % 3;
            profile_set_status(app, "panel page changed");
        }
    }

    profile_update_used_tiles(app);

    if (!ed->open) return;
    if (app->state_machine == ON_COMMAND_BAR_STATE) return;

    if (!ed->drag_selecting && ed->selected_count <= 1 && !profile_tile_is_selected(ed, selected_tile)){
        profile_select_single_tile(ed, selected_tile);
        ed->synced_hash = 0;
    }

    profile_sync_color(app);
    profile_update_filename(ed);

    const int tile_size = (int)(8 * PC_VRAM_TILE_SCALE);
    Vector2 mouse = GetMousePosition();
    Rectangle grid = (Rectangle){0, 0, VRAM_INSPECTOR_WIDTH * tile_size, ((VRAM_TILE_COUNT + VRAM_INSPECTOR_WIDTH - 1) / VRAM_INSPECTOR_WIDTH) * tile_size};
    if (!ed->filename_editing && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, grid)){
        int col = clamp_int((int)(mouse.x / tile_size), 0, VRAM_INSPECTOR_WIDTH - 1);
        int row = clamp_int((int)(mouse.y / tile_size), 0, (VRAM_TILE_COUNT - 1) / VRAM_INSPECTOR_WIDTH);
        int tile = row * VRAM_INSPECTOR_WIDTH + col;
        if (tile >= 0 && tile < VRAM_TILE_COUNT){
            ed->drag_selecting = true;
            ed->drag_start_tile = tile;
            ed->drag_current_tile = tile;
            selected_tile = tile;
            ed->synced_hash = 0;
        }
    }
    if (!ed->filename_editing && ed->drag_selecting && IsMouseButtonDown(MOUSE_LEFT_BUTTON)){
        Vector2 drag_mouse = GetMousePosition();
        drag_mouse.x = drag_mouse.x < grid.x ? grid.x : drag_mouse.x;
        drag_mouse.y = drag_mouse.y < grid.y ? grid.y : drag_mouse.y;
        drag_mouse.x = drag_mouse.x >= grid.x + grid.width ? grid.x + grid.width - 1 : drag_mouse.x;
        drag_mouse.y = drag_mouse.y >= grid.y + grid.height ? grid.y + grid.height - 1 : drag_mouse.y;
        int col = clamp_int((int)(drag_mouse.x / tile_size), 0, VRAM_INSPECTOR_WIDTH - 1);
        int row = clamp_int((int)(drag_mouse.y / tile_size), 0, (VRAM_TILE_COUNT - 1) / VRAM_INSPECTOR_WIDTH);
        int tile = row * VRAM_INSPECTOR_WIDTH + col;
        if (tile >= 0 && tile < VRAM_TILE_COUNT){
            ed->drag_current_tile = tile;
            selected_tile = tile;
            ed->synced_hash = 0;
        }
    }
    if (!ed->filename_editing && ed->drag_selecting && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)){
        int tile = ed->drag_current_tile;
        bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        selected_tile = tile;
        if (ed->drag_start_tile != ed->drag_current_tile){
            profile_select_tile_rect(ed, ed->drag_start_tile, ed->drag_current_tile, ctrl_down);
        }
        else if (shift_down) profile_select_tile_range(ed, tile);
        else if (ctrl_down) profile_toggle_tile_selection(ed, tile);
        else profile_select_single_tile(ed, tile);
        ed->drag_selecting = false;
        ed->drag_start_tile = -1;
        ed->drag_current_tile = -1;
        ed->synced_hash = 0;
        profile_set_status(app, "%d tile%s selected", profile_effective_selection_count(ed), profile_effective_selection_count(ed) == 1 ? "" : "s");
    }

    if (!ed->filename_editing && IsKeyPressed(KEY_M)){
        profile_toggle_tile_selection(ed, selected_tile);
        profile_set_status(app, "%d tile%s selected", profile_effective_selection_count(ed), profile_effective_selection_count(ed) == 1 ? "" : "s");
    }
    if (!ed->filename_editing && IsKeyPressed(KEY_ESCAPE)){
        ed->drag_selecting = false;
        ed->drag_start_tile = -1;
        ed->drag_current_tile = -1;
        profile_select_single_tile(ed, selected_tile);
        profile_set_status(app, "selection reset to tile %d", selected_tile);
    }

    if (IsKeyPressed(KEY_F5)){
        save_meta(ed->filename, app->meta);
        profile_set_status(app, "saved meta/%s", ed->filename);
    }
    if (IsKeyPressed(KEY_F9)){
        load_meta(ed->filename, &app->meta);
        ed->synced_hash = 0;
        profile_set_status(app, "loaded meta/%s", ed->filename);
    }
}

static void profile_draw_z_row(app_state *app, int x, int y, const char *name, int field, uint32_t value){
    char buf[64];
    char tip[96];
    DrawText(name, x, y + 3, 16, WHITE);
    snprintf(tip, sizeof(tip), "Decrease %s depth for the current selection", name);
    if (ui_small_button_tip(x + 130, y, "-", tip)) profile_adjust_z(app, field, -1);
    snprintf(buf, sizeof(buf), "%2u", value);
    DrawText(buf, x + 168, y + 3, 16, WHITE);
    snprintf(tip, sizeof(tip), "Increase %s depth for the current selection", name);
    if (ui_small_button_tip(x + 200, y, "+", tip)) profile_adjust_z(app, field, 1);
}

static void profile_draw_tile_preview(tile_t *tile, int x, int y, int scale, Color tint){
    if (tile == NULL || tile->raw_data == NULL) return;
    for (int row=0; row<TILE_SIZE/2; row++){
        uint8_t lsb = tile->raw_data[row*2];
        uint8_t msb = tile->raw_data[row*2+1];
        for (int bit=0; bit<8; bit++){
            int color_index = ((lsb >> bit) & 1) | (((msb >> bit) & 1) << 1);
            float level = intensity_levels[color_index];
            Color c = (Color){
                (unsigned char)(tint.r * level),
                (unsigned char)(tint.g * level),
                (unsigned char)(tint.b * level),
                255
            };
            DrawRectangle(x + (7-bit)*scale, y + row*scale, scale, scale, c);
        }
    }
    DrawRectangleLines(x, y, 8*scale, 8*scale, WHITE);
}

static void profile_draw_panel(app_state *app){
    profile_editor_t *ed = &app->profile_editor;
    if (!ed->open){
        DrawRectangle(10, GetScreenHeight() - 32, 205, 24, (Color){30, 30, 30, 200});
        DrawText("F1: Profile Editor", 18, GetScreenHeight() - 27, 16, WHITE);
        return;
    }

    int x = GetScreenWidth() - PROFILE_PANEL_WIDTH - PROFILE_PANEL_MARGIN;
    int panel_h = GetScreenHeight() - 2 * PROFILE_PANEL_MARGIN;
    if (x < 270) x = 270;
    int y = PROFILE_PANEL_MARGIN;
    int w = PROFILE_PANEL_WIDTH;
    uint32_t hash = selected_tile_hash();
    meta_t *m = get_meta(app->meta, hash);
    char buf[256];
    ui_tooltip_reset();

    DrawRectangle(x, y, w, panel_h, (Color){24, 27, 31, 235});
    DrawRectangleLines(x, y, w, panel_h, (Color){170, 170, 170, 255});

    y += 8;
    DrawText("3DGB Profile Editor", x + 12, y, 20, WHITE); y += 24;
    DrawText("F1 editor  F2 pause  F3 command  Ctrl+Z/Y", x + 12, y, 12, (Color){210, 210, 210, 255}); y += 19;

    int tab_y = y;
    if (ui_button_tip((Rectangle){x + 12, tab_y, 96, 24}, ed->panel_page == 0 ? "* Edit" : "Edit", "Color, depth and selection editing")) ed->panel_page = 0;
    if (ui_button_tip((Rectangle){x + 116, tab_y, 96, 24}, ed->panel_page == 1 ? "* Profile" : "Profile", "Profile file and legacy command tools")) ed->panel_page = 1;
    if (ui_button_tip((Rectangle){x + 220, tab_y, 104, 24}, ed->panel_page == 2 ? "* Exchange" : "Exchange", "Export and import profile packages")) ed->panel_page = 2;
    y += 31;

    snprintf(buf, sizeof(buf), "Tile %d  Hash %u", selected_tile, hash);
    DrawText(buf, x + 12, y, 14, WHITE); y += 17;
    snprintf(buf, sizeof(buf), "Meta: %s  Total: %d  Selected: %d  Used: %d", m ? "present" : "missing", meta_count(app->meta), profile_effective_selection_count(ed), profile_used_tile_count(ed));
    DrawText(buf, x + 12, y, 14, WHITE);
    if (ui_button_tip((Rectangle){x + w - 92, y - 5, 76, 22}, "Clear", "Reset selection to the current tile")){
        profile_select_single_tile(ed, selected_tile);
        profile_set_status(app, "selection reset to tile %d", selected_tile);
    }
    y += 26;

    if (ed->panel_page == 0){
        DrawText("Emulation", x + 12, y, 15, WHITE);
        snprintf(buf, sizeof(buf), "%s", app->paused ? "Paused" : "Running");
        DrawText(buf, x + 100, y, 15, app->paused ? YELLOW : WHITE);
        y += 20;
        if (ui_button_tip((Rectangle){x + 12, y, 112, 23}, app->paused ? "Resume" : "Pause", "Pause or resume emulation while keeping the editor active")){
            app->paused = !app->paused;
            profile_set_status(app, "emulation %s", app->paused ? "paused" : "running");
        }
        y += 31;

        DrawText("Color target", x + 12, y, 15, WHITE); y += 19;
        if (ui_button_tip((Rectangle){x + 12, y, 62, 23}, "BG", "Edit background tile color")) { ed->color_target = PROFILE_COLOR_BG; ed->synced_hash = 0; }
        if (ui_button_tip((Rectangle){x + 82, y, 62, 23}, "WIN", "Edit window tile color")) { ed->color_target = PROFILE_COLOR_WIN; ed->synced_hash = 0; }
        if (ui_button_tip((Rectangle){x + 152, y, 62, 23}, "OBJ", "Edit object/sprite tile color")) { ed->color_target = PROFILE_COLOR_OBJ; ed->synced_hash = 0; }
        DrawText(target_label(ed->color_target), x + 226, y + 4, 15, YELLOW);
        y += 30;

        Color current = meta_target_color(m, ed->color_target);
        Color pending = (Color){(unsigned char)ed->color_r, (unsigned char)ed->color_g, (unsigned char)ed->color_b, 255};
        DrawText("Preview", x + 12, y, 16, WHITE); y += 20;
        DrawText("Current", x + 12, y, 12, (Color){210, 210, 210, 255});
        DrawText("Pending", x + 124, y, 12, (Color){210, 210, 210, 255});
        y += 15;
        DrawRectangle(x + 12, y, 30, 30, current);
        DrawRectangleLines(x + 12, y, 30, 30, WHITE);
        DrawRectangle(x + 124, y, 30, 30, pending);
        DrawRectangleLines(x + 124, y, 30, 30, WHITE);
        if (selected_tile >= 0 && selected_tile < VRAM_TILE_COUNT){
            profile_draw_tile_preview(&tiles_on_vram[selected_tile], x + 48, y, 4, current);
            profile_draw_tile_preview(&tiles_on_vram[selected_tile], x + 160, y, 4, pending);
        }
        snprintf(buf, sizeof(buf), "RGB %d %d %d", ed->color_r, ed->color_g, ed->color_b);
        DrawText(buf, x + 252, y + 8, 14, WHITE);
        y += 44;

        bool color_changed = false;
        color_changed |= ui_slider(ed, 1, x + 12, y, "R", &ed->color_r); y += 25;
        color_changed |= ui_slider(ed, 2, x + 12, y, "G", &ed->color_g); y += 25;
        color_changed |= ui_slider(ed, 3, x + 12, y, "B", &ed->color_b); y += 28;
        if (color_changed){
            profile_set_status(app, "pending %s color RGB %d %d %d", target_label(ed->color_target), ed->color_r, ed->color_g, ed->color_b);
            if (ed->auto_apply) profile_apply_color(app);
        }
        if (ui_button_tip((Rectangle){x + 12, y, 112, 24}, "Apply", "Apply pending color to the current selection")) profile_apply_color(app);
        if (ui_button_tip((Rectangle){x + 132, y, 104, 24}, "Create meta", "Create metadata entries for the current selection")) profile_create_meta_for_selection(app);
        if (ui_button_tip((Rectangle){x + 244, y, 98, 24}, ed->auto_apply ? "Auto ON" : "Auto OFF", "Apply slider changes immediately when enabled")){
            ed->auto_apply = !ed->auto_apply;
            profile_set_status(app, "auto apply %s", ed->auto_apply ? "enabled" : "disabled");
        }
        y += 32;

        DrawText("Z depth", x + 12, y, 16, WHITE); y += 21;
        uint32_t all_v = m ? m->bg_for_z : 0;
        profile_draw_z_row(app, x + 12, y, "All", 5, all_v); y += 23;
        profile_draw_z_row(app, x + 12, y, "BG front", 0, m ? m->bg_for_z : 0); y += 23;
        profile_draw_z_row(app, x + 12, y, "BG back", 1, m ? m->bg_back_z : 0); y += 23;
        profile_draw_z_row(app, x + 12, y, "Window", 2, m ? m->win_z : 0); y += 23;
        profile_draw_z_row(app, x + 12, y, "Object", 3, m ? m->obj_z : 0); y += 23;
        profile_draw_z_row(app, x + 12, y, "Obj behind", 4, m ? m->obj_behind_z : 0); y += 29;

        bool draw_obj_c0 = m && (m->flags & DRAW_OBJ_C0);
        if (ui_button_tip((Rectangle){x + 12, y, 190, 24}, draw_obj_c0 ? "OBJ color 0: ON" : "OBJ color 0: OFF", "Toggle rendering of object palette color 0")){
            profile_set_flag_for_selection(app, DRAW_OBJ_C0, !draw_obj_c0);
        }
        y += 34;

        DrawText("Last change", x + 12, y, 15, WHITE); y += 18;
        DrawRectangle(x + 12, y, w - 24, 34, (Color){33, 36, 42, 255});
        DrawRectangleLines(x + 12, y, w - 24, 34, (Color){100, 100, 100, 255});
        DrawText(ed->last_change, x + 20, y + 8, 13, WHITE);
        y += 43;
    }
    else if (ed->panel_page == 1){
        DrawText("Profile file", x + 12, y, 16, WHITE); y += 21;
        Rectangle textbox = (Rectangle){x + 12, y, w - 24, 28};
        Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, textbox)) ed->filename_editing = true;
        DrawRectangleRec(textbox, ed->filename_editing ? (Color){40, 46, 58, 255} : (Color){35, 35, 35, 255});
        DrawRectangleLines((int)textbox.x, (int)textbox.y, (int)textbox.width, (int)textbox.height, ed->filename_editing ? YELLOW : WHITE);
        snprintf(buf, sizeof(buf), "%s%s", ed->filename, ed->dirty ? " *" : "");
        DrawText(buf, (int)textbox.x + 8, (int)textbox.y + 6, 16, WHITE);
        y += 38;
        if (ui_button_tip((Rectangle){x + 12, y, 86, 28}, "Save", "Save the current profile to meta/")){
            save_meta(ed->filename, app->meta);
            profile_mark_saved(app);
            profile_set_status(app, "saved meta/%s", ed->filename);
        }
        if (ui_button_tip((Rectangle){x + 108, y, 86, 28}, "Load", "Load this profile from meta/")){
            if (ed->dirty && ed->confirm_load == 0){
                ed->confirm_load = 1;
                profile_set_status(app, "unsaved changes; click Load again to discard");
            }
            else{
                load_meta(ed->filename, &app->meta);
                profile_after_load(app);
                profile_set_status(app, "loaded meta/%s", ed->filename);
            }
        }
        if (ui_button_tip((Rectangle){x + 204, y, 78, 28}, "Undo", "Undo the last profile edit")) profile_undo(app);
        if (ui_button_tip((Rectangle){x + 292, y, 78, 28}, "Redo", "Redo the last undone edit")) profile_redo(app);
        y += 42;
        snprintf(buf, sizeof(buf), "Current meta entries: %d", meta_count(app->meta));
        DrawText(buf, x + 12, y, 15, WHITE); y += 20;
        snprintf(buf, sizeof(buf), "Unsaved changes: %d%s", ed->dirty ? ed->unsaved_changes : 0, ed->dirty ? "" : " (saved)");
        DrawText(buf, x + 12, y, 14, ed->dirty ? YELLOW : (Color){190, 220, 190, 255}); y += 24;
        DrawText("Command bar", x + 12, y, 16, WHITE); y += 21;
        DrawText("F3 opens the command bar", x + 20, y, 13, (Color){210, 210, 210, 255}); y += 17;
        DrawText("Type a command and press Enter", x + 20, y, 13, (Color){210, 210, 210, 255}); y += 17;
        DrawText("Examples: set_meta bg_color r g b", x + 20, y, 12, (Color){190, 190, 190, 255}); y += 16;
        DrawText("save_meta file.meta / load_meta file.meta", x + 20, y, 12, (Color){190, 190, 190, 255}); y += 24;
        DrawText("Selection controls", x + 12, y, 16, WHITE); y += 21;
        DrawText("Click: single tile", x + 20, y, 14, (Color){210, 210, 210, 255}); y += 18;
        DrawText("Drag: rectangle selection", x + 20, y, 14, (Color){210, 210, 210, 255}); y += 18;
        DrawText("Ctrl + click/drag: add or toggle", x + 20, y, 14, (Color){210, 210, 210, 255}); y += 18;
        DrawText("Shift + click: range from anchor", x + 20, y, 14, (Color){210, 210, 210, 255}); y += 18;
        DrawText("M toggles current tile, Esc resets", x + 20, y, 14, (Color){210, 210, 210, 255}); y += 30;
        DrawText("Last change", x + 12, y, 15, WHITE); y += 18;
        DrawRectangle(x + 12, y, w - 24, 42, (Color){33, 36, 42, 255});
        DrawRectangleLines(x + 12, y, w - 24, 42, (Color){100, 100, 100, 255});
        DrawText(ed->last_change, x + 20, y + 9, 13, WHITE);
        y += 53;
    }
    else{
        DrawText("Exchange", x + 12, y, 16, WHITE); y += 22;
        if (ui_button_tip((Rectangle){x + 12, y, 126, 26}, "Export package", "Export frame, tile data and current profile")) profile_export_package(app);
        if (ui_button_tip((Rectangle){x + 148, y, 118, 26}, "Preview import", "Read imports/profile_import.json and write import_preview.json")) profile_load_import_entries(app);
        if (ui_button_tip((Rectangle){x + 276, y, 112, 26}, "Apply import", "Apply the loaded import entries to the profile")) profile_apply_import_entries(app);
        y += 34;
        snprintf(buf, sizeof(buf), "Import source: imports/%s", ed->exchange_filename);
        DrawText(buf, x + 12, y, 13, (Color){210, 210, 210, 255}); y += 18;
        snprintf(buf, sizeof(buf), "Loaded: %d", ed->import_entry_count);
        DrawText(buf, x + 12, y, 13, (Color){210, 210, 210, 255}); y += 18;
        snprintf(buf, sizeof(buf), "Existing %d  New %d", ed->import_preview_existing, ed->import_preview_new);
        DrawText(buf, x + 12, y, 13, (Color){210, 210, 210, 255}); y += 18;
        snprintf(buf, sizeof(buf), "Changed entries %d  Field changes %d", ed->import_preview_changed_entries, ed->import_preview_field_changes);
        DrawText(buf, x + 12, y, 13, (Color){210, 210, 210, 255}); y += 18;
        snprintf(buf, sizeof(buf), "Preview file: %s", ed->import_preview_path);
        DrawText(buf, x + 12, y, 12, (Color){180, 180, 180, 255}); y += 26;
        DrawText("Last export", x + 12, y, 16, WHITE); y += 20;
        DrawRectangle(x + 12, y, w - 24, 48, (Color){33, 36, 42, 255});
        DrawRectangleLines(x + 12, y, w - 24, 48, (Color){100, 100, 100, 255});
        DrawText(ed->last_export_path, x + 20, y + 8, 12, WHITE);
        y += 60;
        DrawText("Export package includes:", x + 12, y, 15, WHITE); y += 19;
        DrawText("window snapshot, frame image, tile grid", x + 20, y, 13, (Color){210, 210, 210, 255}); y += 17;
        DrawText("all_tiles folder, CSV/JSON and profile", x + 20, y, 13, (Color){210, 210, 210, 255}); y += 20;
        DrawText("Import accepts tile_hash or hash.", x + 12, y, 13, (Color){210, 210, 210, 255}); y += 17;
        DrawText("Colors can be arrays or RGB strings.", x + 12, y, 13, (Color){210, 210, 210, 255}); y += 24;
    }

    int status_y = PROFILE_PANEL_MARGIN + panel_h - 48;
    DrawRectangle(x + 12, status_y, w - 24, 38, (Color){35, 35, 35, 255});
    DrawRectangleLines(x + 12, status_y, w - 24, 38, (Color){100, 100, 100, 255});
    DrawText(ed->status, x + 20, status_y + 8, 13, WHITE);
    ui_tooltip_draw();
}

void ray_init(app_state *app){
    (void)app;
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1100, 760, "3DGB");
	SetTargetFPS(60);
	
    // Initialise raylib camera
	camera_distance = 10.0f;
	camera = (Camera3D){
		(Vector3){0, 0, camera_distance},
		(Vector3){0, 0, 0},
		(Vector3){0, 1, 0},
		60.0f,
		CAMERA_PERSPECTIVE
	};
}

static void __draw_framebuffers(app_state *app){
	static Texture buffers_textures[256];
    static int buffers_textures_i = 0;
    
    BeginMode3D(camera);
	
    for (int i=0; i<Z_LAYERS; i++){
		
		framebuffer_t *fb = &app->framebuffers[i];
		if (!fb->used_flag && fb->copy == NULL){
			continue;
		}

		float z = i*app->planes_distance;
		uint32_t *pixels = &fb->pixels[0][0];
		if (fb->copy != NULL)
			pixels = &fb->copy->pixels[0][0];

        // CONVERT FRAMEBUFFER INTO A TEXTURE
        if (buffers_textures[buffers_textures_i].id == 0){
            Image img = (Image){
                pixels,
                LCD_WIDTH, LCD_HEIGHT,
                1,
                PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
            };

            buffers_textures[buffers_textures_i] = LoadTextureFromImage(img);
        }

        else{
            UpdateTexture(
                buffers_textures[buffers_textures_i], 
                pixels
            );
        }
    
        // RENDER THE FRAMEBUFFER AS A BILLBOARD
        DrawBillboard(
            camera, 
            buffers_textures[buffers_textures_i], 
            (Vector3){
                camera.target.x, 
                camera.target.y, 
                camera.target.z + z
            },
            3.0, 
            WHITE
        );

        // ADVANCE THE TEXTURE COUNTER
        buffers_textures_i++;
        if (buffers_textures_i >= 256)
            buffers_textures_i = 0;
    }

	EndMode3D();
}

static void __draw_tile(tile_t *t, int x, int y, float scale){
	static Texture tiles_textures[VRAM_TILE_COUNT];
    static int tiles_textures_i = 0;

    uint32_t fb[8][8];
	
	// PARSE THE TILE DATA INTO AN R8G8B8A8 IMAGE
    // *each line of the tile is 2 bytes long*
	for (int i=0; i<TILE_SIZE/2; i++){
		uint8_t lsb = t->raw_data[i*2];
		uint8_t msb = t->raw_data[i*2+1];

		for (int o=0; o<8; o++){
			int color_index  = ((lsb >> o) & 1) | (((msb >> o) & 1) << 1);			
            Color color = (Color){
				255 * intensity_levels[color_index],
				255 * intensity_levels[color_index],
				255 * intensity_levels[color_index],
				255
			};

            memcpy(&fb[i][7-o], &color, sizeof(uint32_t));
		}

	}

	// CONVERT PREVIOUS IMAGE INTO A TEXTURE
	if (tiles_textures[tiles_textures_i].id == 0){
		Image i = (Image){
			&(fb[0][0]),
			8, 8,
			1,
			PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
		};

		tiles_textures[tiles_textures_i] = LoadTextureFromImage(i);
	}

	else{
		UpdateTexture(tiles_textures[tiles_textures_i], &(fb[0][0]));
	}

	// RENDER THE TILE TEXTURE
	DrawTexturePro(
		tiles_textures[tiles_textures_i],
		(Rectangle){0,0,8,8},
		(Rectangle){x,y,8*scale, 8*scale},
		(Vector2){0,0},
		0, WHITE
	);
	
    // ADVANCE OR REVERSE THE TEXTURE COUNTER
    tiles_textures_i++;
    if (tiles_textures_i >= VRAM_TILE_COUNT)
        tiles_textures_i = 0;

}

static void __draw_vram_tiles(app_state *app, tile_t *tiles, int x, int y, int line_width, float scale){
	int x_offset = 0;
	int y_offset = 0;
	int tiles_online = 0;
	int tiles_drawn = 0;
	for (int i=0; i<VRAM_TILE_COUNT; i++){
		tile_t *t = &tiles[i];
		int tx = x + x_offset;
		int ty = y + y_offset;
		int tw = (int)(8*scale);
		__draw_tile(t, tx, ty, scale);

		if (app->profile_editor.open && app->profile_editor.used_tiles[tiles_drawn]){
			DrawRectangleLines(tx + 1, ty + 1, tw - 2, tw - 2, (Color){70, 220, 110, 255});
		}

		if (app->profile_editor.open && app->profile_editor.drag_selecting && profile_tile_in_rect(tiles_drawn, app->profile_editor.drag_start_tile, app->profile_editor.drag_current_tile)){
			DrawRectangle(tx, ty, tw, tw, (Color){90, 150, 255, 55});
			DrawRectangleLines(tx, ty, tw, tw, SKYBLUE);
		}
		if (app->profile_editor.open && profile_has_tile_in_selection(&app->profile_editor, tiles_drawn)){
			DrawRectangleLines(tx, ty, tw, tw, ORANGE);
			DrawRectangleLines(tx + 1, ty + 1, tw - 2, tw - 2, ORANGE);
		}
		if (tiles_drawn == selected_tile){
			DrawRectangleLines(tx, ty, tw, tw, PINK);
		}
		x_offset += 8*scale;
		
		// line break
		tiles_online++;
		tiles_drawn++;
		if (tiles_online >= line_width){
			x_offset = 0;
			y_offset += 8*scale;
			tiles_online = 0;
		}
	}
}

static void __ray_draw(app_state *app){
	BeginDrawing();
	ClearBackground(BG_COLOR);

	__draw_framebuffers(app);

	// DRAW THE TILES INSPECTOR
	__draw_vram_tiles(
		app,
		tiles_on_vram, 
		0, 0, 
		VRAM_INSPECTOR_WIDTH, 
		PC_VRAM_TILE_SCALE
	);

	// DRAW COMMAND-BAR
	if (app->state_machine == ON_COMMAND_BAR_STATE){
		DrawRectangle(
			0, 0, 
			GetScreenWidth(), 20, 
			(Color){20,20,20,255}
		);

		DrawText(app->commandbar.text, 0, 0, 20, WHITE);
	}

    if (app->paused){
        DrawRectangle(10, GetScreenHeight() - 64, 250, 26, (Color){38, 38, 38, 220});
        DrawRectangleLines(10, GetScreenHeight() - 64, 250, 26, YELLOW);
        DrawText("PAUSED - editing snapshot", 20, GetScreenHeight() - 59, 16, YELLOW);
    }

    profile_draw_panel(app);

	EndDrawing();
}

void ray_update(app_state *app){
    profile_update(app);
    __ray_draw(app);
}
