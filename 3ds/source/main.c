#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <tex3ds.h>
#include <string.h>
#include "3ds/allocator/linear.h"
#include "3ds/gpu/enums.h"
#include "3ds/os.h"
#include "3ds/services/hid.h"
#include "c3d/maths.h"
#include "c3d/renderqueue.h"
#include "c3d/texture.h"
#include "meta.h"
#include "vshader_shbin.h"
#include "main.h"
#include "utils.h"
#include "peanut_gb.h"
#include "lcd.h"

#define CLEAR_COLOR 0x68B0D8FF
#define TARGET_SPEED_US (1000000.0 / VERTICAL_SYNC)
#define FRAMEBUFFER_BACKUPS 3

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct vertex { 
	float position[3]; 
	float texcoord[2]; 
} vertex_t;

/* <== Various =================================================> */
C3D_RenderTarget *target_l, *target_r;
float BASE_PLANE_DISTANCE = -0.49f;
float PLANES_DISTANCE = 0.0178f;
float slider, iod;
app_state app;
static TickCounter cnt, cnt1;
state_t state_machine;
char entry_name[256];

/* <== Shader ==================================================> */
static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int projection_uniform, modelView_uniform; // uniforms

/* <== Objects =================================================> */
static void* vbo;
static vertex_t plane_mesh[] = {
	// first triangle
	{ {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f} },
	{ {+0.5f, -0.5f, 0.0f}, {1.0f, 0.0f} },
	{ {+0.5f, +0.5f, 0.0f}, {1.0f, 1.0f} },
	// fecond triangle
	{ {+0.5f, +0.5f, 0.0f}, {1.0f, 1.0f} },
	{ {-0.5f, +0.5f, 0.0f}, {0.0f, 1.0f} },
	{ {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f} },
};

C3D_Tex textures[Z_LAYERS*FRAMEBUFFER_BACKUPS];
int backup = 0;
uint32_t swizzle_table[256*256];

/* <== Framebuffers ============================================> */

static inline void reset_framebuffers(app_state *app, int backup){
	for (int i=0; i<Z_LAYERS; i++){
		if (!app->framebuffers[i].used_flag) continue;
		app->framebuffers[i].copy = NULL;
		app->framebuffers[i].used_flag = false;
		
		C3D_Tex *tex = get_framebuffer_tex(&app->framebuffers[i],backup);
		memset(tex->data, 0, sizeof(uint32_t)*256*256);
	}
}

static inline void set_planes_distance(float distance){
	// Create the VBO (vertex buffer object)
	for (int i=0; i<Z_LAYERS; i++){
		int inv = (Z_LAYERS - 1) - i;

		for (int vi=0; vi<6; vi++)
			plane_mesh[vi].position[2] = distance * inv * -1;

		memcpy(
			((u8*)vbo) + sizeof(plane_mesh)*i,
			plane_mesh,
			sizeof(plane_mesh)
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

static int app_init(app_state *app, char* rom_filename){
	memset(app, 0, sizeof(*app));
	
	// Copy input ROM file to allocated memory (esto aloja memoria)
	app->rom = read_rom_to_ram(rom_filename);
	if (app->rom == NULL){
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
	size_t card_ram_size = gb_get_save_size(&app->gb);
	app->cart_ram = calloc(1, card_ram_size);

	// Init LCD
	gb_init_lcd(&app->gb, &lcd_render_line);
	app->gb.direct.interlace = true;
	app->gb.direct.frame_skip = true;

	// Init framebuffers
	for (int i=0; i<Z_LAYERS; i++){
		app->framebuffers[i].tex = &textures[i];
	}
	
	for (int i=0; i<FRAMEBUFFER_BACKUPS; i++){
		reset_framebuffers(app,i);
	}
	
	return 0;
}

/* <== Drawing =================================================> */

inline static void draw_plane(float distance, int copies, C3D_Tex *tex){
	C3D_Mtx modelView;
	Mtx_Identity(&modelView);
	Mtx_Translate(&modelView, 0.0, 0.0, BASE_PLANE_DISTANCE + distance, true);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, modelView_uniform, &modelView); //update the uniform
	
	C3D_TexBind(0, tex);
	int vertex_count = 6*Z_LAYERS;
	int count = copies * 6;
	int first = vertex_count - count;
	//C3D_DrawArrays(GPU_TRIANGLES, start, count);
	C3D_DrawArrays(GPU_TRIANGLES,  first, count);
}

static void draw_scene(float iod){	
	// Compute the projection matrix
	C3D_Mtx projection;

	// Empirical calibration factor: in our setup Mtx_PerspStereoTilt()'s 'screen' parameter
	// (zero-parallax plane) does NOT map 1:1 to our world/view Z units.
	// 0.717 was measured so that geometry at BASE_PLANE_DISTANCE has ~0 horizontal parallax.
	Mtx_PerspStereoTilt(
		&projection, 
		C3D_AngleFromDegrees(80.0f), 
		C3D_AspectRatioTop, 
		0.01f, 1000.0f, 
		iod,  0.717 *(BASE_PLANE_DISTANCE*-1), 
		false
	);

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_uniform, &projection);
	
	for (int i=0; i<Z_LAYERS; i++){
		framebuffer_t *fb = &app.framebuffers[i];
		if (!fb->used_flag && fb->copy == NULL)
			continue;
		
		int k=backup+1;
		if (k >= FRAMEBUFFER_BACKUPS) k=0;
		for (int j=0; j<FRAMEBUFFER_BACKUPS; j++){
			draw_plane(PLANES_DISTANCE*i, i+1, get_framebuffer_tex(fb->copy != NULL ? fb->copy:fb, k));
			k++;
			if (k >= FRAMEBUFFER_BACKUPS) k=0;
		}
	}

}

/* <== Others ==================================================> */

static void graphics_init(void){
	/* <== Graphics ================================================> */
	gfxInitDefault();
	
	// Initialize console
	PrintConsole bottomScreen;
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);
	
	// Initialize Citro3D
	gfxSet3D(true); // Enable stereoscopic 3D
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C3D_AlphaTest(true, GPU_GREATER, 0);
	
	// Create renderTargets
	target_r = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	target_l = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(target_r, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(target_l, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	projection_uniform   = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	modelView_uniform    = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");

	// Configure attributes for use with the vertex shader
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
	
	// config the first fragment shading substage to blend the texture color with the vertex color
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

	// Create the VBO (vertex buffer object)
	vbo = linearAlloc(sizeof(plane_mesh)*Z_LAYERS);
	set_planes_distance(PLANES_DISTANCE);

	// Configure buffers
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, vbo, sizeof(vertex_t), 2, 0x210);

	// Textures setup
	C3D_TexInitParams params = {
		256, 256, 1,
		GPU_RGBA8, GPU_TEX_2D,
		false
	};
	
	for (int i=0; i<Z_LAYERS*FRAMEBUFFER_BACKUPS; i++){
		if (!C3D_TexInitWithParams(&textures[i], NULL, params))
			svcBreak(USERBREAK_PANIC);
		C3D_TexSetFilter(&textures[i], GPU_NEAREST, GPU_NEAREST);
	}

	get_swizzle_table(swizzle_table, 256, 256);
}

static void cleanup(void){
	// Free the textures
	for (int i=0; i<Z_LAYERS*FRAMEBUFFER_BACKUPS; i++){
		C3D_TexDelete(&textures[i]);
	}

	// Free the VBO
	linearFree(vbo);
	
	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);

	// Free the render targets
	C3D_RenderTargetDelete(target_r);
	C3D_RenderTargetDelete(target_l);

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();

	free_meta(app.meta);
}

void handle_input(){
	hidScanInput();
	u32 k_held = hidKeysHeld();
	
	app.gb.direct.joypad = 255; //clean joypad state
	if (k_held & KEY_RIGHT)  app.gb.direct.joypad &= ~JOYPAD_RIGHT;
	if (k_held & KEY_LEFT)   app.gb.direct.joypad &= ~JOYPAD_LEFT;
	if (k_held & KEY_UP)     app.gb.direct.joypad &= ~JOYPAD_UP;
	if (k_held & KEY_DOWN)   app.gb.direct.joypad &= ~JOYPAD_DOWN;
	if (k_held & KEY_A)      app.gb.direct.joypad &= ~JOYPAD_A;
	if (k_held & KEY_B)      app.gb.direct.joypad &= ~JOYPAD_B;
	if (k_held & KEY_START)  app.gb.direct.joypad &= ~JOYPAD_START;
	if (k_held & KEY_SELECT) app.gb.direct.joypad &= ~JOYPAD_SELECT;

	if (k_held & KEY_X)   BASE_PLANE_DISTANCE += 0.001f;
	if (k_held & KEY_Y)   BASE_PLANE_DISTANCE -= 0.001f;
	if (k_held & KEY_R)   set_planes_distance(PLANES_DISTANCE += 0.0001f);
	if (k_held & KEY_L)   set_planes_distance(PLANES_DISTANCE -= 0.0001f);

}

static void render(void){
	// TOP SCREEN
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	osTickCounterStart(&cnt);
	
	C3D_RenderTargetClear(target_l, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_FrameDrawOn(target_l);
	draw_scene(-iod);

	if (iod > 0){
		C3D_RenderTargetClear(target_r, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
		C3D_FrameDrawOn(target_r);
		draw_scene(iod);
	}
		
	C3D_FrameEnd(0);
}

static int main_loop(bool render){
	/* <== Always logic ============================================> */
	handle_input();

	// Reads 3D slider
	slider = osGet3DSliderState();
	iod = slider/5;

	/* <== Emulator logic ==========================================> */
	app.gb.direct.frame_skip = !render;
	if (app.paused) return 0;
		gb_run_frame(&app.gb);

	return 0;
}

int menu_entry(char *path, char *msg, int offset){
	DIR* dir = opendir(path);
	if (!dir) return -1;
	
	struct dirent* ent;
	int entries=0;
	
	// clear screen, set header
	printf("\x1b[2J \x1b[00;00H%s", msg);
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_type == DT_DIR) continue;
		printf("\x1b[%02i;02H   %s", entries+offset, ent->d_name);
		entries++;
	}

	// set first *
	printf("\x1b[%02i;00H xx", offset);

	closedir(dir);
	return entries;
}

void select_entry(char *path, int option){
	memset(entry_name, '\0', 256);
	
	DIR* dir = opendir(path);
	if (!dir) return;
	
	struct dirent* ent;
	int entries=0;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_type == DT_DIR) continue;
		if (entries == option){
			sprintf(entry_name, "%s%s", path, ent->d_name);
			break;
		}
		entries++;
	}

	closedir(dir);
}

int main(){
	graphics_init();

	int offset = 3;
	int options_q = menu_entry("sdmc:/", "Select room: | A:SELECT", offset);
	int selected_option = 0;
	while (aptMainLoop()){

		if (state_machine == SELECTING_ROM){
			hidScanInput();
			u32 k_down = hidKeysDown();
			if (k_down & KEY_DOWN){
				printf("\x1b[%02i;00H%s", selected_option+offset, "    ");
				selected_option++;
				if (selected_option >= options_q)
					selected_option = options_q-1;
				printf("\x1b[%02i;00H%s", selected_option+offset, " xx");
			}
			
			if (k_down & KEY_UP){
				printf("\x1b[%02i;00H%s", selected_option+offset, "    ");
				selected_option--;
				if (selected_option < 0)
					selected_option = 0;
				printf("\x1b[%02i;00H%s", selected_option+offset, " xx");
			}
			
			if (k_down & KEY_A){
				select_entry("sdmc:/", selected_option);
				if (app_init(&app, entry_name))
					break;
				
				options_q = menu_entry("sdmc:/", "Select meta: | A:SELECT  B:CANCEL", offset);
				selected_option = 0;
				state_machine = SELECTING_META;
			}

			svcSleepThread(20 * 1000 * 1000LL);
		}

		if (state_machine == SELECTING_META){
			hidScanInput();
			u32 k_down = hidKeysDown();
			if (k_down & KEY_DOWN){
				printf("\x1b[%02i;00H%s", selected_option+offset, "    ");
				selected_option++;
				if (selected_option >= options_q)
					selected_option = options_q-1;
				printf("\x1b[%02i;00H%s", selected_option+offset, " xx");
			}
			
			if (k_down & KEY_UP){
				printf("\x1b[%02i;00H%s", selected_option+offset, "    ");
				selected_option--;
				if (selected_option < 0)
					selected_option = 0;
				printf("\x1b[%02i;00H%s", selected_option+offset, " xx");
			}
			
			if (k_down & KEY_A){
				select_entry("sdmc:/", selected_option);
				load_meta(entry_name, &app.meta);
				state_machine = GB_RUNNING_STATE;
			}

			if (k_down & KEY_B)
				state_machine = GB_RUNNING_STATE;

			svcSleepThread(20 * 1000 * 1000LL);
		}

		if (state_machine == GB_RUNNING_STATE){
			static double emulation_time = 0;
			static double render_time = 0;
			static double total_time = 0;
			static const double frame_us = 1000000.0 / 60.0;
			static double debt_us = frame_us;

			osTickCounterStart(&cnt);
			osTickCounterStart(&cnt1);
			
			reset_framebuffers(&app, backup);
			
			// Catch up emulation if needed
			while (debt_us >= frame_us) {
				bool draw = (debt_us < 2*frame_us); //render just the last one
				if (main_loop(draw)) break;
				debt_us -= frame_us;
			}
			
			//GSPGPU_FlushDataCache(pixels,sizeof(uint32_t)*256*256*Z_LAYERS);
			osTickCounterUpdate(&cnt);
			emulation_time = osTickCounterRead(&cnt) * 1000.0;

			render(); // osTickCounterStart gets called inside
			backup += 1;
			if (backup >= FRAMEBUFFER_BACKUPS)
				backup = 0;

			osTickCounterUpdate(&cnt);
			osTickCounterUpdate(&cnt1);
			render_time = osTickCounterRead(&cnt) * 1000.0;
			total_time = osTickCounterRead(&cnt1) * 1000.0;
			debt_us += total_time;

			static int framecount = 0;
			framecount += 1;
			if (framecount >= 10){
				printf("\x1b[2J");// clear screen
				printf("\x1b[01;00Hemulation_time       :%f", emulation_time);
				printf("\x1b[02;00Hrender_time          :%f", render_time);
				printf("\x1b[03;00Htotal_time           :%f", total_time);
				printf("\x1b[04;00Hwasted_time          :%f", total_time - render_time - emulation_time);
				printf("\x1b[05;00Hbase_plane_distance  :%f", BASE_PLANE_DISTANCE);
				printf("\x1b[06;00Hinter_planes_distance:%f", PLANES_DISTANCE);
				framecount = 0;
			}
		}

	}

	cleanup();
	return 0;
}