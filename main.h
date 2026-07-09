#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <raylib.h>
#include "peanut_gb.h"
#include "meta.h"

#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#define SCREEN_SCALE 3.0
#define VRAM_TILE_COUNT 384
#define TILE_SIZE 16

#define VRAM_INSPECTOR_WIDTH 10
#define Z_LAYERS 50
#define PROFILE_IMPORT_MAX_ENTRIES 512
#define PROFILE_HISTORY_MAX 32
#define PROFILE_HISTORY_MAX_META 1024
#define PLANES_DISTANCE_DEFAULT 0.2f
#define BG_COLOR CLITERAL(Color){ 230, 224, 210, 255 }

typedef enum{
	GB_RUNNING_STATE,
	ON_COMMAND_BAR_STATE
} state_t;

typedef enum profile_color_target{
	PROFILE_COLOR_BG = 0,
	PROFILE_COLOR_WIN = 1,
	PROFILE_COLOR_OBJ = 2
} profile_color_target_t;

typedef struct tile{
	uint8_t *raw_data;
	uint32_t hash;
} tile_t;

typedef struct commandbar{
	char text[256];
	int text_len;
	int cursor;
} commandbar_t;

typedef struct framebuffer{
    uint32_t pixels[LCD_HEIGHT][LCD_WIDTH];
    bool used_flag;
	struct framebuffer *copy;
} framebuffer_t;


typedef struct profile_meta_state{
	uint32_t tile_hash;
	Color bg_color, win_color, obj_color;
	uint32_t bg_for_z, bg_back_z;
	uint32_t win_z, obj_z, obj_behind_z;
	uint32_t flags;
} profile_meta_state_t;

typedef struct profile_history_entry{
	int count;
	char label[64];
	profile_meta_state_t entries[PROFILE_HISTORY_MAX_META];
} profile_history_entry_t;

typedef struct profile_import_entry{
	uint32_t tile_hash;
	bool has_bg_color;
	bool has_win_color;
	bool has_obj_color;
	bool has_bg_for_z;
	bool has_bg_back_z;
	bool has_win_z;
	bool has_obj_z;
	bool has_obj_behind_z;
	bool has_flags;
	Color bg_color;
	Color win_color;
	Color obj_color;
	uint32_t bg_for_z;
	uint32_t bg_back_z;
	uint32_t win_z;
	uint32_t obj_z;
	uint32_t obj_behind_z;
	uint32_t flags;
} profile_import_entry_t;

typedef struct profile_editor{
	bool open;
	bool filename_editing;
	bool drag_selecting;
	int drag_start_tile;
	int drag_current_tile;
	profile_color_target_t color_target;
	int color_r;
	int color_g;
	int color_b;
	bool auto_apply;
	bool selected_tiles[VRAM_TILE_COUNT];
	bool used_tiles[VRAM_TILE_COUNT];
	int selected_count;
	int selection_anchor;
	char filename[128];
	char status[160];
	char last_change[160];
	char exchange_filename[128];
	char export_dir[192];
	profile_import_entry_t import_entries[PROFILE_IMPORT_MAX_ENTRIES];
	int import_entry_count;
	bool import_loaded;
	int import_preview_existing;
	int import_preview_new;
	int import_preview_changed_entries;
	int import_preview_field_changes;
	char import_preview_path[192];
	char last_export_path[256];
	int panel_page;
	bool dirty;
	int unsaved_changes;
	int confirm_load;
	int value_editing_id;
	char value_edit_text[8];
	profile_history_entry_t *undo_stack;
	profile_history_entry_t *redo_stack;
	int undo_count;
	int redo_count;
	uint32_t synced_hash;
	profile_color_target_t synced_target;
} profile_editor_t;

// rom, cart_ram y fb pertenecen a una pseudo estructura "priv" que gb espera
// esos deberían estar dentro de gb_s creo
typedef struct app_state{
	uint8_t *rom;                       // Pointer to allocated memory holding GB file.
	uint8_t *cart_ram;                  // Pointer to allocated memory holding save file.
	framebuffer_t *framebuffers;        // Frame buffers
	float planes_distance;
	state_t state_machine;
	bool paused;
	commandbar_t commandbar;
	profile_editor_t profile_editor;
	meta_t *meta;                       // Tiles metadata linked list
	gb_s gb;                            // Emulator context
} app_state;

extern const float intensity_levels[];
extern tile_t tiles_on_vram[VRAM_TILE_COUNT];
extern int selected_tile;

//void sort_framebuffers_by_z(app_state *app);
void draw_to_framebuffer(app_state *app, uint32_t z, int x, int y, Color color);
void profile_push_undo(app_state *app, const char *label);
void profile_mark_dirty(app_state *app, const char *label);
void profile_mark_saved(app_state *app);
void profile_after_load(app_state *app);

#endif
