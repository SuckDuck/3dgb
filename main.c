#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include "peanut_gb.h"
#include "main.h"
#include "lcd.h"
#include "raylib_backend.h"
#include "meta.h"

// I don't know exactly what to do with this
// later i will determine
tile_t tiles_on_vram[VRAM_TILE_COUNT];
int selected_tile = 0;
float planes_distance = 0.2f;

/* <== Utils ===================================================> */

void sample_vram_tiles(gb_s *gb, tile_t *tiles){
	for (int i=0; i<VRAM_TILE_COUNT; i++){
		tiles[i].raw_data = &gb->vram[TILE_SIZE*i];
		tiles[i].hash = ComputeCRC32(
			tiles[i].raw_data, 
			TILE_SIZE
		);
	}
}

void handle_input(app_state *app){
	app->gb.direct.joypad = 255; //clean joypad state
	if (IsKeyDown(KEY_RIGHT))     app->gb.direct.joypad &= ~JOYPAD_RIGHT;
	if (IsKeyDown(KEY_LEFT))      app->gb.direct.joypad &= ~JOYPAD_LEFT;
	if (IsKeyDown(KEY_UP))        app->gb.direct.joypad &= ~JOYPAD_UP;
	if (IsKeyDown(KEY_DOWN))      app->gb.direct.joypad &= ~JOYPAD_DOWN;
	if (IsKeyDown(KEY_Z))         app->gb.direct.joypad &= ~JOYPAD_A;
	if (IsKeyDown(KEY_X))         app->gb.direct.joypad &= ~JOYPAD_B;
	if (IsKeyDown(KEY_P))         app->gb.direct.joypad &= ~JOYPAD_START;
	if (IsKeyDown(KEY_BACKSPACE)) app->gb.direct.joypad &= ~JOYPAD_SELECT;
}

void exec_cmd(app_state *app, int argc, char **argv){
	if (argc == 0) return;
	
	// PRINT COMMAND
	printf("COMMAND:");
	for (int i=0; i<argc; i++)
		printf("%s ", argv[i]);
	printf("\n");

	// SET META COMMAND
	if (!strcmp(argv[0], "set_meta")){
		if (argc < 2){
			printf("ERROR:%s\n", "bad format");
			return;
		}

		// BACKGROUND COLOR
		if (!strcmp(argv[1], "bg_color")){
			if (argc != 5){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			Color c = (Color){
				atoi(argv[2]),
				atoi(argv[3]),
				atoi(argv[4]),
				255
			};
			
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				&c, NULL, 
				NULL, NULL, 
				NULL, NULL,
				NULL, NULL
			);

			return;		
		}

		// WINDOW COLOR
		else if (!strcmp(argv[1], "win_color")){
			if (argc != 5){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			Color c = (Color){
				atoi(argv[2]),
				atoi(argv[3]),
				atoi(argv[4]),
				255
			};
			
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, &c, 
				NULL, NULL, 
				NULL, NULL,
				NULL, NULL
			);

			return;
		}

		// OBJECT COLOR
		else if (!strcmp(argv[1], "obj_color")){
			if (argc != 5){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			Color c = (Color){
				atoi(argv[2]),
				atoi(argv[3]),
				atoi(argv[4]),
				255
			};
			
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, NULL, 
				&c, NULL, 
				NULL, NULL,
				NULL, NULL
			);

			return;
		}

		// ALL Z
		else if (!strcmp(argv[1], "all_z")){
			if (argc != 3){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			uint32_t z = (uint32_t)strtoul(argv[2], NULL, 10);
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, NULL, 
				NULL, &z, 
				&z, &z,
				&z, &z
			);
		}

		// BG FOR Z DEPTH
		else if (!strcmp(argv[1], "bg_for_z")){
			if (argc != 3){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			uint32_t z = (uint32_t)strtoul(argv[2], NULL, 10);
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, NULL, 
				NULL, &z, 
				NULL, NULL,
				NULL, NULL
			);

			return;
		}

		// BG BACK Z DEPTH
		else if (!strcmp(argv[1], "bg_back_z")){
			if (argc != 3){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			uint32_t z = (uint32_t)strtoul(argv[2], NULL, 10);
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, NULL, 
				NULL, NULL, 
				&z, NULL,
				NULL, NULL
			);

			return;
		}

		// WIN Z DEPTH
		else if (!strcmp(argv[1], "win_z")){
			if (argc != 3){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			uint32_t z = (uint32_t)strtoul(argv[2], NULL, 10);
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, NULL, 
				NULL, NULL, 
				NULL, &z,
				NULL, NULL
			);

			return;
		}

		// OBJ Z DEPTH
		else if (!strcmp(argv[1], "obj_z")){
			if (argc != 3){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			uint32_t z = (uint32_t)strtoul(argv[2], NULL, 10);
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, NULL, 
				NULL, NULL, 
				NULL, NULL,
				&z, NULL
			);

			return;
		}

		// OBJ BEHIND Z DEPTH
		else if (!strcmp(argv[1], "obj_behind_z")){
			if (argc != 3){
				printf("ERROR:%s\n", "bad format");
				return;
			}

			uint32_t z = (uint32_t)strtoul(argv[2], NULL, 10);
			set_meta(
				&app->meta, 
				tiles_on_vram[selected_tile].hash, 
				NULL, NULL, 
				NULL, NULL, 
				NULL, NULL,
				NULL, &z
			);

			return;
		}

		
		printf("ERROR:%s\n", "bad format");
		return;
	}

	// SAVE META COMMAND
	else if (!strcmp(argv[0], "save_meta")){
		if (argc != 2){
			printf("ERROR:%s\n", "bad format");
			return;
		}

		save_meta(argv[1], app->meta);
	}

	// LOAD META COMMAND
	else if (!strcmp(argv[0], "load_meta")){
		if (argc != 2){
			printf("ERROR:%s\n", "bad format");
			return;
		}

		load_meta(argv[1], &app->meta);
	}

}

void reset_framebuffers(app_state *app){
	memset( &app->framebuffers[0], 
		0, sizeof(framebuffer_t)*Z_LAYERS
	);
}

void draw_to_framebuffer(app_state *app, uint32_t z, int x, int y, Color color){
	// Copy color to the framebuffer
	framebuffer_t *fb = &app->framebuffers[z];
	fb->used_flag = true;
	memcpy(&fb->pixels[y][x], &color, sizeof(uint32_t));
	return;
}

void compose_framebuffers(framebuffer_t *over, framebuffer_t *behind){
	if (!behind->used_flag){
		if (over->copy != NULL)
			behind->copy = over->copy;
		else behind->copy = over;
		return;
	}

	if (over->copy != NULL)
		over = over->copy;

	for (int y=0; y<LCD_HEIGHT; y++){
		for (int x=0; x<LCD_WIDTH; x++){
			Color over_pixel_color;
			memcpy(&over_pixel_color, &over->pixels[y][x], sizeof(Color));
			if (over_pixel_color.a == 0) continue;
			behind->pixels[y][x] = over->pixels[y][x];
		}
	}
}

void compose_all_framebuffers(app_state *app){
	// FROM FRONT TO BACK
    for (int i = Z_LAYERS - 1; i > 0; i--) {
        compose_framebuffers(
            &app->framebuffers[i],
            &app->framebuffers[i - 1]
        );
    }
}

/* <== Callbacks ===============================================> */

uint8_t gb_rom_read(gb_s *gb, const uint_fast32_t addr){
	// Returns a byte from the ROM file at the given address.
	const struct app_state * const p = gb->direct.priv;
	return p->rom[addr];
}

uint8_t gb_cart_ram_read(gb_s *gb, const uint_fast32_t addr){
	// Returns a byte from the cartridge RAM at the given address.
	const struct app_state * const p = gb->direct.priv;
	return p->cart_ram[addr];
}

void gb_cart_ram_write(gb_s *gb, const uint_fast32_t addr, const uint8_t val){
	// Writes a given byte to the cartridge RAM at the given address.
	const struct app_state * const p = gb->direct.priv;
	p->cart_ram[addr] = val;
}

uint8_t *read_rom_to_ram(const char *file_name){
	// Returns a pointer to the allocated space containing the ROM. Must be freed.
	FILE *rom_file = fopen(file_name, "rb");
	size_t rom_size;
	uint8_t *rom = NULL;

	if(rom_file == NULL)
		return NULL;

	fseek(rom_file, 0, SEEK_END);
	rom_size = ftell(rom_file);
	rewind(rom_file);
	rom = malloc(rom_size);

	if(fread(rom, sizeof(uint8_t), rom_size, rom_file) != rom_size)
	{
		free(rom);
		fclose(rom_file);
		return NULL;
	}

	fclose(rom_file);
	return rom;
}

void gb_error(gb_s *gb, const enum gb_error_e gb_err, const uint16_t val){
	// Ignore all errors.
	const char* gb_err_str[GB_INVALID_MAX] = {
		"UNKNOWN",
		"INVALID OPCODE",
		"INVALID READ",
		"INVALID WRITE",
		"HALT FOREVER"
	};
	struct app_state *priv = gb->direct.priv;

	fprintf(stderr, "Error %d occurred: %s at %04X\n. Exiting.\n",
			gb_err, gb_err_str[gb_err], val);

	/* Free memory and then exit. */
	free(priv->cart_ram);
	free(priv->rom);
	exit(EXIT_FAILURE);
}

/* <== Frontend ================================================> */

static int on_gb_running_state(app_state *app){
	if (app->paused) return 0;
	const double target_speed_us = 1000000.0 / VERTICAL_SYNC;
	int_fast16_t delay;
	unsigned long start, end;
	struct timeval timecheck;
	int state;

	gettimeofday(&timecheck, NULL);
	start = (long)timecheck.tv_sec * 1000000 +
		(long)timecheck.tv_usec;

	handle_input(app);

	/* Execute CPU cycles until the screen has to be redrawn. */
	gb_run_frame(&app->gb);

	gettimeofday(&timecheck, NULL);
	end = (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;

	// Transition to commandBar
	if (IsKeyPressed(KEY_SPACE)){
		app->state_machine = ON_COMMAND_BAR_STATE;
		return 0;
	}
	
	delay = target_speed_us - (end - start);

	/* If it took more than the maximum allowed time to draw frame,
	* do not delay.
	* Interlaced mode could be enabled here to help speed up
	* drawing.
	*/

	if(delay < 0) return 0;
	usleep(delay);

	// TILES INSPECTOR LOGIC
	sample_vram_tiles(&app->gb, tiles_on_vram);
	if (IsKeyPressed(KEY_D) && selected_tile < VRAM_TILE_COUNT-1)
		selected_tile++;
	if (IsKeyPressed(KEY_S) && selected_tile < VRAM_TILE_COUNT-VRAM_INSPECTOR_WIDTH)
		selected_tile+=VRAM_INSPECTOR_WIDTH;
	if (IsKeyPressed(KEY_A) && selected_tile > 0)
		selected_tile--;
	if (IsKeyPressed(KEY_W) && selected_tile >= VRAM_INSPECTOR_WIDTH)
		selected_tile-=VRAM_INSPECTOR_WIDTH;

	return 0;
}

static void on_command_bar_state(app_state *app){
	// ADD CHARACTERS
	char pressedChar;
    while (true) {
        pressedChar = GetCharPressed();
        if (pressedChar == 0) break;
        app->commandbar.text[app->commandbar.cursor++] = pressedChar;
    }

	// DELETE CHARACTERS
    if (app->commandbar.text[0] != '\0' && IsKeyPressed(KEY_BACKSPACE)){
        app->commandbar.text[app->commandbar.cursor-1] = '\0';
        app->commandbar.cursor--;
    }

	// CANCEL COMMAND
    if (IsKeyPressed(KEY_ESCAPE)){
        goto end;
    }

	// ACCEPT COMMAND
	if (IsKeyPressed(KEY_ENTER)){
		char *argv[32];
		int argc = 0;
		while (true){
			argv[argc] = strtok(argc > 0 ? NULL:app->commandbar.text," ");
			if (argv[argc] == NULL) break;
			argc++;
		}
		
		exec_cmd(app, argc, argv);
		goto end;
	}

	return;
	
	end:
	memset(app->commandbar.text, '\0', 256);
	app->commandbar.text_len = 0;
	app->commandbar.cursor = 0;
	app->state_machine = GB_RUNNING_STATE;
	return;
}

static int main_loop(app_state *app){
	switch (app->state_machine){
		case GB_RUNNING_STATE:     on_gb_running_state(app);  break;
		case ON_COMMAND_BAR_STATE: on_command_bar_state(app); break;
	}

	return 0;
}

static int init(app_state *app, char* rom_filename){
	memset(app, 0, sizeof(*app));
	
	// Copy input ROM file to allocated memory (esto aloja memoria)
	app->rom = read_rom_to_ram(rom_filename);
	if (app->rom == NULL){
		printf("%d: %s\n", __LINE__, strerror(errno));
		return EXIT_FAILURE;
	}

	// Initialise context
	gb_init_error_e ret;
	ret = gb_init(
		&app->gb, 
		&gb_rom_read, 
		&gb_cart_ram_read, 
		&gb_cart_ram_write, 
		&gb_error, 
		app
	);

	if(ret != GB_INIT_NO_ERROR){
		printf("Error: %d\n", ret);
		return EXIT_FAILURE;
	}

	// Initialise card ram
	size_t card_ram_size;
	gb_get_save_size_s(&app->gb, &card_ram_size);
	app->cart_ram = calloc(1, card_ram_size);

	// Init LCD
	gb_init_lcd(&app->gb, &lcd_render_line);
	//app->gb.direct.interlace = true;
	//app->gb.direct.frame_skip = true;

	// Init framebuffers
	app->planes_distance = PLANES_DISTANCE_DEFAULT;
	app->framebuffers = malloc(sizeof(framebuffer_t)*Z_LAYERS);
	reset_framebuffers(app);

	return 0;
}

static void shutdown(app_state *app){
	free(app->framebuffers);
	free(app->cart_ram);
	free(app->rom);
}

int main(int argc, char **argv){
	// Rom name reading
	char *rom_filename = NULL;
	if (argc == 2) rom_filename = argv[1];
	else{
		fprintf(stderr, "%s ROM\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	
	// App initialise
	app_state app;
	int ret = init(&app, rom_filename) != 0;
	if (ret != 0) return ret;

	ray_init(&app);

	// App pipeline
	printf("p1 = %p\n",&app);
	while(!WindowShouldClose()){
		if (main_loop(&app) != 0) break;
		compose_all_framebuffers(&app);
		ray_update(&app);
		if (app.state_machine == GB_RUNNING_STATE){
			reset_framebuffers(&app);
		}
	}

	// App end
	shutdown(&app);
	CloseWindow();

	return EXIT_SUCCESS;
}
