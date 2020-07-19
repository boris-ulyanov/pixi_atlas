
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pixi_atlas_frame {
    const char* name;
    uint16_t x, y, w, h;
} pixi_atlas_frame;

typedef struct pixi_atlas {
    const char* image;      // meta/image
    uint16_t image_w;       // meta/size/w
    uint16_t image_h;       // meta/size/h
    uint16_t frames_size;   // frames count
    pixi_atlas_frame* frames;
} pixi_atlas;

pixi_atlas* pixi_atlas_parse(const char* filename);

void pixi_atlas_free(pixi_atlas* atlas);

void pixi_atlas_dump(const pixi_atlas* atlas);

#ifdef __cplusplus
}
#endif
