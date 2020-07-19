
#include "../pixi_atlas.h"

#include <stdio.h>

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        printf("usage: %s atlas.json... \n", argv[0]);
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        pixi_atlas* atlas = pixi_atlas_parse(argv[i]);
        pixi_atlas_dump(atlas);
        pixi_atlas_free(atlas);
    }

    return 0;
}
