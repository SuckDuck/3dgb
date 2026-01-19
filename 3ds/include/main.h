#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <3ds.h>
#include <citro3d.h>
#include "c3d/texture.h"
#include "peanut_gb.h"
#include "meta.h"
#include "utils.h"

#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#define SCREEN_SCALE 3.0
#define VRAM_TILE_COUNT 384
#define TILE_SIZE 16

#define VRAM_INSPECTOR_WIDTH 10
#define Z_LAYERS 15
#define BG_COLOR CLITERAL(Color){ 230, 224, 210, 255 }

typedef enum{
	SELECTING_ROM,
	SELECTING_META,
	GB_RUNNING_STATE,
} state_t;

typedef struct tile{
	uint8_t *raw_data;
	uint32_t hash;
} tile_t;

typedef struct framebuffer{
    C3D_Tex *tex;
	bool used_flag;
	struct framebuffer *copy;
} framebuffer_t;

// rom, cart_ram y fb pertenecen a una pseudo estructura "priv" que gb espera
// esos deberían estar dentro de gb_s creo
typedef struct app_state{
	uint8_t *rom;                         // Pointer to allocated memory holding GB file.
	uint8_t *cart_ram;                    // Pointer to allocated memory holding save file.
	framebuffer_t framebuffers[Z_LAYERS]; // Frame buffers
	bool paused;
	meta_t *meta;                         // Tiles metadata linked list
	gb_s gb;                              // Emulator context
} app_state;

extern const float intensity_levels[];
extern tile_t tiles_on_vram[VRAM_TILE_COUNT];
extern int selected_tile;
extern uint32_t swizzle_table[256*256];
extern int backup;

static inline C3D_Tex *get_framebuffer_tex(framebuffer_t *fb, int backup){
	return fb->tex+(Z_LAYERS*backup);
}

static inline void draw_to_framebuffer(app_state *app, uint32_t z, int x, int y, Color color){
	// Copy color to the framebuffer
	framebuffer_t *fb = &app->framebuffers[z];
	fb->used_flag = true;
	color = (Color){color.a, color.b, color.g, color.r};
	
	int x_offset = 48;
	int y_offset = 56;
	
	C3D_Tex *tex = get_framebuffer_tex(fb, backup);
	((uint32_t*) tex->data)[swizzle_table[(y+y_offset)*256 + x + x_offset]] = *(uint32_t*)&color;

	return;
}

#endif