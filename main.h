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
#define PLANES_DISTANCE_DEFAULT 0.2f
#define BG_COLOR CLITERAL(Color){ 230, 224, 210, 255 }

typedef enum{
	GB_RUNNING_STATE,
	ON_COMMAND_BAR_STATE
} state_t;

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
	meta_t *meta;                       // Tiles metadata linked list
	gb_s gb;                            // Emulator context
} app_state;

extern const float intensity_levels[];
extern tile_t tiles_on_vram[VRAM_TILE_COUNT];
extern int selected_tile;

//void sort_framebuffers_by_z(app_state *app);
void draw_to_framebuffer(app_state *app, uint32_t z, int x, int y, Color color);

#endif