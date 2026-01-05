#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include "peanut_gb.h"
#include "main.h"

const float intensity_levels[] = {
	1.0f, 0.66f, 0.33f, 0.0f
};

/* <== Utils ===================================================> */

static int compare_sprites(const struct sprite_data *const sd1, const struct sprite_data *const sd2){
	int x_res;

	x_res = (int)sd1->x - (int)sd2->x;
	if(x_res != 0)
		return x_res;

	return (int)sd1->sprite_number - (int)sd2->sprite_number;
}

bool check_interlaced_line_skip(gb_s *gb){
	if (!gb->direct.interlace) return false;
	if ((!gb->display.interlace_count && (gb->hram_io[IO_LY] & 1) == 0) || 
		(gb->display.interlace_count  && (gb->hram_io[IO_LY] & 1) == 1) ){
		
		/* Compensate for missing window draw if required. */
		if (gb->hram_io[IO_LCDC] & LCDC_WINDOW_ENABLE && 
			gb->hram_io[IO_LY] >= gb->display.WY && 
			gb->hram_io[IO_WX] <= 166){
			
			gb->display.window_clear++;
		}

		return true;
	}

	return false;
}

/* <== Render ==================================================> */

void render_background_line(gb_s *gb, uint8_t *pixels){
    if (!(gb->hram_io[IO_LCDC] & LCDC_BG_ENABLE)) return;
    
    app_state *app = gb->direct.priv;
    uint8_t bg_y, disp_x, bg_x, idx, py, px, t1, t2;
    uint16_t bg_map, tile;
    uint32_t tile_hash;
    meta_t *meta = NULL;

    // Calculate current background line to draw. Constant because
    // this function draws only this one line each time it is
    // called.
    bg_y = gb->hram_io[IO_LY] + gb->hram_io[IO_SCY];

    // Get selected background map address for first tile
    // corresponding to current line.
    // 0x20 (32) is the width of a background tile, and the bit
    // shift is to calculate the address.
    bg_map = ((gb->hram_io[IO_LCDC] & LCDC_BG_MAP) ? VRAM_BMAP_2 : VRAM_BMAP_1) + (bg_y >> 3) * 0x20;

    // The displays (what the player sees) X coordinate, drawn right
    // to left.
    disp_x = LCD_WIDTH - 1;

    // The X coordinate to begin drawing the background at.
    bg_x = disp_x + gb->hram_io[IO_SCX];

    // Get tile index for current background tile.
    idx = gb->vram[bg_map + (bg_x >> 3)];
    // Y coordinate of tile pixel to draw.
    py = (bg_y & 0x07);
    // X coordinate of tile pixel to draw.
    px = 7 - (bg_x & 0x07);

    // Select addressing mode.
    if (gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT){
        tile = VRAM_TILES_1 + idx * 0x10;
    }	
    else {
        tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
    }
    
    // fetch first tile
    tile_hash = ComputeCRC32(&gb->vram[tile], TILE_SIZE);
    meta = get_meta(app->meta, tile_hash);
    tile += 2 * py;
    t1 = gb->vram[tile] >> px;
    t2 = gb->vram[tile + 1] >> px;

    for (; disp_x != 0xFF; disp_x--){
        uint8_t c;
        if (px == 8){
            // fetch next tile
            px = 0;
            bg_x = disp_x + gb->hram_io[IO_SCX];
            idx = gb->vram[bg_map + (bg_x >> 3)];

            if (gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT){
                tile = VRAM_TILES_1 + idx * 0x10;
            }
            else {
                tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
            }
                
            tile_hash = ComputeCRC32(&gb->vram[tile], TILE_SIZE);
            meta = get_meta(app->meta, tile_hash);
            tile += 2 * py;
            t1 = gb->vram[tile];
            t2 = gb->vram[tile + 1];
            
        }

        // write palette data to pixels 
        // (used later because of transparency and sprite priority)
        c = (t1 & 0x1) | ((t2 & 0x1) << 1);
        pixels[disp_x] = gb->display.bg_palette[c];
        
        // blit bg line to frame buffer (back)
        uint32_t tile_back_z = 0;
        float back_intensity = intensity_levels[gb->display.bg_palette[0] & 3];
        Color back_color = (Color){
            back_intensity * 255,
            back_intensity * 255,
            back_intensity * 255,
            255
        };

        if (meta != NULL){
            tile_back_z = meta->bg_back_z;
            back_color = ColorLerp(meta->bg_color, WHITE, back_intensity);
        }
            
        draw_to_framebuffer(app, tile_back_z, disp_x, bg_y, back_color);

        // blit bg line to frame buffer (front)
        if (c > 0){
            float front_intensity = intensity_levels[pixels[disp_x] & 3];
            Color pixel_color = (Color){
                front_intensity * 255, 
                front_intensity * 255, 
                front_intensity * 255, 
                255
            };
        
            // apply custom metadata to tile
            uint32_t tile_front_z = 0;
            if (meta != NULL){
                pixel_color = ColorLerp(meta->bg_color, WHITE, front_intensity);
                tile_front_z = meta->bg_for_z;
            }
            draw_to_framebuffer(app, tile_front_z, disp_x, bg_y, pixel_color);
        }
        
        t1 = t1 >> 1;
        t2 = t2 >> 1;
        px++;
    }
}

void render_window_line(gb_s *gb, uint8_t *pixels){
    if (!(gb->hram_io[IO_LCDC] & LCDC_WINDOW_ENABLE && gb->hram_io[IO_LY] >= gb->display.WY && gb->hram_io[IO_WX] <= 166 )) 
        return;

    app_state *app = gb->direct.priv;
    uint16_t win_line, tile;
    uint8_t disp_x, win_x, py, px, idx, t1, t2, end;
    uint32_t tile_hash;
    meta_t *meta = NULL;
    
    int line_y = gb->hram_io[IO_LY];

    // Calculate Window Map Address.
    win_line = (gb->hram_io[IO_LCDC] & LCDC_WINDOW_MAP) ?
                VRAM_BMAP_2 : VRAM_BMAP_1;
    win_line += (gb->display.window_clear >> 3) * 0x20;

    disp_x = LCD_WIDTH - 1;
    win_x = disp_x - gb->hram_io[IO_WX] + 7;

    // look up tile
    py = gb->display.window_clear & 0x07;
    px = 7 - (win_x & 0x07);
    idx = gb->vram[win_line + (win_x >> 3)];

    if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT)
        tile = VRAM_TILES_1 + idx * 0x10;
    else
        tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;

    // fetch first tile
    tile_hash = ComputeCRC32(&t1, TILE_SIZE);
    meta = get_meta(app->meta, tile_hash);
    tile += 2 * py;
    t1 = gb->vram[tile] >> px;
    t2 = gb->vram[tile + 1] >> px;
    

    // loop & copy window
    end = (gb->hram_io[IO_WX] < 7 ? 0 : gb->hram_io[IO_WX] - 7) - 1;

    for(; disp_x != end; disp_x--)
    {
        uint8_t c;

        if(px == 8)
        {
            // fetch next tile
            px = 0;
            win_x = disp_x - gb->hram_io[IO_WX] + 7;
            idx = gb->vram[win_line + (win_x >> 3)];

            if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT)
                tile = VRAM_TILES_1 + idx * 0x10;
            else
                tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;

            tile_hash = ComputeCRC32(&t1, TILE_SIZE);
            meta = get_meta(app->meta, tile_hash);
            tile += 2 * py;
            t1 = gb->vram[tile];
            t2 = gb->vram[tile + 1];
        }

        // write palette data to pixels 
        // (used later because of transparency and sprite priority)
        c = (t1 & 0x1) | ((t2 & 0x1) << 1);
        pixels[disp_x] = gb->display.bg_palette[c];

        // blit win line to frame buffer
        float intensity = intensity_levels[pixels[disp_x] & 3];
        Color pixel_color = (Color){
            intensity * 255, 
            intensity * 255, 
            intensity * 255, 
            255
        };
        
        // apply custom metadata to tile
        uint32_t tile_z = 0;
        if (meta != NULL){
            pixel_color = ColorLerp(meta->win_color, WHITE, intensity);
            tile_z = meta->win_z;
        }
            
        draw_to_framebuffer(app, tile_z, disp_x, line_y, pixel_color);

        t1 = t1 >> 1;
        t2 = t2 >> 1;
        tile_hash = ComputeCRC32(&t1, TILE_SIZE);
        px++;
    }

    gb->display.window_clear++; // advance window line
}

void render_sprites_line(gb_s *gb, uint8_t *pixels){
    if(!(gb->hram_io[IO_LCDC] & LCDC_OBJ_ENABLE)) return;

    app_state *app = gb->direct.priv;
    uint8_t sprite_number;
    uint32_t tile_hash;
    meta_t *meta;

    int line_y = gb->hram_io[IO_LY];
    #if PEANUT_GB_HIGH_LCD_ACCURACY
        uint8_t number_of_sprites = 0;

        struct sprite_data sprites_to_render[MAX_SPRITES_LINE];

        // Record number of sprites on the line being rendered, limited
        // to the maximum number sprites that the Game Boy is able to
        // render on each line (10 sprites).
        for (sprite_number = 0; sprite_number < NUM_SPRITES; sprite_number++){
            // Sprite Y position.
            uint8_t OY = gb->oam[4 * sprite_number + 0];
            // Sprite X position.
            uint8_t OX = gb->oam[4 * sprite_number + 1];

            // If sprite isn't on this line, continue.
            if (line_y + (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0 : 8) >= OY || line_y + 16 < OY ){
                continue;
            }
                
            struct sprite_data current;
            current.sprite_number = sprite_number;
            current.x = OX;

            uint8_t place;
            for (place = number_of_sprites; place != 0; place--){
                if(compare_sprites(&sprites_to_render[place - 1], &current) < 0)
                    break;
            }

            if(place >= MAX_SPRITES_LINE)
                continue;
            
            for (uint8_t i = number_of_sprites; i > place; --i){
                sprites_to_render[i] = sprites_to_render[i - 1];
            }

            if(number_of_sprites < MAX_SPRITES_LINE)
                number_of_sprites++;
            
            sprites_to_render[place] = current;
        }
    #endif

    // Render each sprite, from low priority to high priority.
    #if PEANUT_GB_HIGH_LCD_ACCURACY
        // Render the top ten prioritised sprites on this scanline.
        for(sprite_number = number_of_sprites - 1;
                sprite_number != 0xFF;
                sprite_number--)
        {
            uint8_t s = sprites_to_render[sprite_number].sprite_number;
    #else
        for (sprite_number = NUM_SPRITES - 1;
            sprite_number != 0xFF;
            sprite_number--)
        {
            uint8_t s = sprite_number;
    #endif
        uint8_t py, t1, t2, dir, start, end, shift, disp_x;
        // Sprite Y position.
        uint8_t OY = gb->oam[4 * s + 0];
        // Sprite X position.
        uint8_t OX = gb->oam[4 * s + 1];
        // Sprite Tile/Pattern Number.
        uint8_t OT = gb->oam[4 * s + 2]
                    & (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0xFE : 0xFF);
        // Additional attributes.
        uint8_t OF = gb->oam[4 * s + 3];

        #if !PEANUT_GB_HIGH_LCD_ACCURACY
            // If sprite isn't on this line, continue.
            if(line_y +
                    (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0 : 8) >= OY ||
                    line_y + 16 < OY)
                continue;
        #endif

        // Continue if sprite not visible.
        if(OX == 0 || OX >= 168)
            continue;

        // y flip
        py = line_y - OY + 16;

        if(OF & OBJ_FLIP_Y)
            py = (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 15 : 7) - py;

        // fetch the tile
        tile_hash = ComputeCRC32(&(gb->vram[OT * 0x10]), TILE_SIZE);
        meta = get_meta(app->meta, tile_hash);
        t1 = gb->vram[VRAM_TILES_1 + OT * 0x10 + 2 * py];
        t2 = gb->vram[VRAM_TILES_1 + OT * 0x10 + 2 * py + 1];
        
        // handle x flip
        if(OF & OBJ_FLIP_X)
        {
            dir = 1;
            start = (OX < 8 ? 0 : OX - 8);
            end = MIN(OX, LCD_WIDTH);
            shift = 8 - OX + start;
        }
        else
        {
            dir = (uint8_t)-1;
            start = MIN(OX, LCD_WIDTH) - 1;
            end = (OX < 8 ? 0 : OX - 8) - 1;
            shift = OX - (start + 1);
        }

        // copy tile
        t1 >>= shift;
        t2 >>= shift;

        // TODO: Put for loop within the to if statements
        // because the BG priority bit will be the same for
        // all the pixels in the tile.
        for(disp_x = start; disp_x != end; disp_x += dir){
            uint8_t c = (t1 & 0x1) | ((t2 & 0x1) << 1);
            // check transparency / sprite overlap / background overlap

            //if(c && !(OF & OBJ_PRIORITY && !((pixels[disp_x] & 0x3) == gb->display.bg_palette[0]))){
            if(c || (meta != NULL && (meta->flags & DRAW_OBJ_C0))){
                bool render_behind = OF & OBJ_PRIORITY;
            
                // blit sprites line to frame buffer
                float intensity = intensity_levels[
                    (OF & OBJ_PALETTE) ? gb->display.sp_palette[c + 4] : gb->display.sp_palette[c]
                ];

                Color pixel_color = (Color){
                    intensity*255, 
                    intensity*255, 
                    intensity*255, 
                    255
                };

                // apply custom metadata to tile
                uint32_t tile_z = 0;
                if (meta != NULL){
                    pixel_color = ColorLerp(meta->obj_color, WHITE, intensity);
                    tile_z = meta->obj_z;
                    if (render_behind) 
                        tile_z = meta->obj_behind_z;
                }

                draw_to_framebuffer(app, tile_z, disp_x, line_y, pixel_color);
            }

            t1 = t1 >> 1;
            t2 = t2 >> 1;
        }
    }
}

void lcd_render_line(gb_s *gb){
	if (gb->direct.frame_skip && !gb->display.frame_skip_count) return;
	if (check_interlaced_line_skip(gb)) return;
	
	uint8_t pixels[LCD_WIDTH] = {0};
	for (int i=0; i<2; i++){
        render_background_line(gb, pixels);
        render_window_line(gb, pixels);
        render_sprites_line(gb, pixels);
    }
}