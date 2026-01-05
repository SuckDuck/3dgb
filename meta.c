#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "meta.h"

void set_meta(
	meta_t **meta, 
	uint32_t hash, 
	Color *bg_c, Color *win_c, Color *obj_c,
	uint32_t *bg_for_z, uint32_t *bg_back_z, 
	uint32_t *win_z, uint32_t *obj_z,
	uint32_t *obj_behind_z ){
	
		while (true){
			if (*meta == NULL){
				*meta = malloc(1*sizeof(meta_t));
				(*meta)->tile_hash = hash;
				(*meta)->next = NULL;
				(*meta)->bg_color = BLACK;
				(*meta)->win_color = BLACK;
				(*meta)->obj_color = BLACK;
				(*meta)->bg_for_z = 0;
				(*meta)->bg_back_z = 0;
				(*meta)->win_z = 0;
				(*meta)->obj_z = 0;
				(*meta)->obj_behind_z = 0;
				(*meta)->flags = 0;

				printf("New meta created for tile:%u\n", hash);
				break;
			}
			
			if ((*meta)->tile_hash == hash)
				break;
			
			meta = &(*meta)->next;
		}
		
		if (bg_c != NULL)      (*meta)->bg_color = *bg_c;       // BACKGROUND COLOR
		if (win_c != NULL)     (*meta)->win_color = *win_c;     // WINDOW COLOR
		if (obj_c != NULL)     (*meta)->obj_color = *obj_c;     // OBJECT COLOR
		if (bg_for_z != NULL)  (*meta)->bg_for_z = *bg_for_z;   // BACKGROUND Z FORWARD
		if (bg_back_z != NULL) (*meta)->bg_back_z = *bg_back_z; // BACKGROUND Z BACK
		if (win_z != NULL)     (*meta)->win_z = *win_z;         // WINDOW Z
		if (obj_z != NULL)     (*meta)->obj_z = *obj_z;         // OBJECT Z

		// OBJECT BEHIND Z
		if (obj_behind_z != NULL) (*meta)->obj_behind_z = *obj_behind_z;

		printf("Meta updated for tile:%u\n", hash);
}

void meta_add_flags(meta_t *m, uint32_t flags) {
    m->flags |= flags;
}

void meta_clear_flags(meta_t *m, uint32_t flags) {
    m->flags &= ~flags;
}

void meta_set_flags(meta_t *m, uint32_t flags) {
    m->flags = flags;
}

meta_t *get_meta(meta_t *meta, uint32_t hash){
    for (meta_t *m = meta; m != NULL; m=m->next){
        if (m->tile_hash == hash) return m;
    }
    return NULL;
}

void free_meta(meta_t *meta){
	for (meta_t *m=meta; m != NULL;){
		meta_t *prev = m;
		m=m->next;
		free(prev);
	}
}

void save_meta(char* filename, meta_t *meta){
	char meta_path[6 + strlen(filename)];
	sprintf(meta_path, "meta/%s", filename);
	int fd = open(meta_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0){
		printf("file '%s' could not be created\n", meta_path);
		return;
	}

	// STORE THE VERSION
	char version_buf[10] = {0};
	strcpy(version_buf, VERSION);
	write(fd, version_buf, 10);

	// STORE THE META COUNT
	uint32_t meta_q = 0;
	for (meta_t *m=meta; m != NULL; m=m->next)
		meta_q++;
	write(fd, &meta_q, sizeof(uint32_t));

	// STORE EACH META
	for (meta_t *m=meta; m != NULL; m=m->next){
		write(fd, &m->tile_hash, sizeof(uint32_t));
		write(fd, &m->bg_color, sizeof(Color));
		write(fd, &m->win_color, sizeof(Color));
		write(fd, &m->obj_color, sizeof(Color));
		write(fd, &m->bg_for_z, sizeof(uint32_t));
		write(fd, &m->bg_back_z, sizeof(uint32_t));
		write(fd, &m->win_z, sizeof(uint32_t));
		write(fd, &m->obj_z, sizeof(uint32_t));
		write(fd, &m->obj_behind_z, sizeof(uint32_t));
		write(fd, &m->flags, sizeof(uint32_t));
    }

	close(fd);
}

void load_meta_0_1_0(int fd, meta_t **meta){
	printf("loading_meta_0_1_0\n");
	free_meta(*meta);

	// READ THE META COUNT
	uint32_t meta_q;
	read(fd, &meta_q, sizeof(uint32_t));

	// READ EACH META STRUCTURE
	for (int i=0; i<meta_q; i++){
		*meta = malloc(sizeof(meta_t));
		(*meta)->next = NULL;
		read(fd, &(*meta)->tile_hash, sizeof(uint32_t));
		read(fd, &(*meta)->bg_color, sizeof(Color));
		read(fd, &(*meta)->win_color, sizeof(Color));
		read(fd, &(*meta)->obj_color, sizeof(Color));
		read(fd, &(*meta)->bg_for_z, sizeof(uint32_t));
		read(fd, &(*meta)->bg_back_z, sizeof(uint32_t));
		read(fd, &(*meta)->win_z, sizeof(uint32_t));
		read(fd, &(*meta)->obj_z, sizeof(uint32_t));
		read(fd, &(*meta)->obj_behind_z, sizeof(uint32_t));
		read(fd, &(*meta)->flags, sizeof(uint32_t));
		meta = &(*meta)->next;
	}
}

void load_meta(char* filename, meta_t **meta){
	char meta_path[6 + strlen(filename)];
	sprintf(meta_path, "meta/%s", filename);
	int fd = open(meta_path, O_RDONLY);
	if (fd < 0){
		printf("file '%s' could not be opened\n", meta_path);
		return;
	}

	char version_buf[10] = {0};
	read(fd, version_buf, 10);
	if (strcmp(version_buf, "0_1_0") == 0){
		load_meta_0_1_0(fd, meta);
	}
	
	close(fd);
}