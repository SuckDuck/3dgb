#include <raylib.h>
#include <stdint.h>
#include <string.h>
#include "main.h"
#include "peanut_gb.h"

float camera_distance = 10.0f;
Camera3D camera;

void ray_init(app_state *app){
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(200, 200, "3DGB");
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

static void __draw_vram_tiles(tile_t *tiles, int x, int y, int line_width, float scale){
	int x_offset = 0;
	int y_offset = 0;
	int tiles_online = 0;
	int tiles_drawn = 0;
	for (int i=0; i<VRAM_TILE_COUNT; i++){
		tile_t *t = &tiles[i];
		__draw_tile(t, x + x_offset, y + y_offset, scale);
		// Render selected tile outline
		if (tiles_drawn == selected_tile){
			DrawRectangleLines(
				x+x_offset, 
				y+y_offset, 
				8*scale, 
				8*scale, 
				PINK
			);
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
		tiles_on_vram, 
		0, 0, 
		VRAM_INSPECTOR_WIDTH, 
		3
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

	EndDrawing();
}

void ray_update(app_state *app){
    __ray_draw(app);
}