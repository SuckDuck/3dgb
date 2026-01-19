#ifndef META_H
#define META_H

#include <stdint.h>
#include "utils.h"

#ifndef VERSION
#define VERSION "0_0_0"
#endif

typedef enum meta_flags {
	DRAW_OBJ_C0 = 1 << 0
} meta_flags;

typedef struct meta{
	uint32_t tile_hash;
	Color bg_color, win_color, obj_color;
    uint32_t bg_for_z, bg_back_z;
    uint32_t win_z, obj_z, obj_behind_z;
	uint32_t flags;
	struct meta *next;
} meta_t;

void set_meta(
	meta_t **meta, 
	uint32_t hash, 
	Color *bg_c, Color *win_c, Color *obj_c,
	uint32_t *bg_for_z, uint32_t *bg_back_z, 
	uint32_t *win_z, uint32_t *obj_z,
	uint32_t *obj_behind_z 
);

void meta_add_flags(meta_t *m, uint32_t flags);
void meta_clear_flags(meta_t *m, uint32_t flags);
void meta_set_flags(meta_t *m, uint32_t flags);

meta_t *get_meta(meta_t *meta, uint32_t hash);
void free_meta(meta_t *meta);
void save_meta(char* filename, meta_t *meta);
int load_meta(char* filename, meta_t **meta);

#endif