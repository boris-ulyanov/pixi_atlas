
#include "pixi_atlas.h"

#include "jsmn.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const jsmntok_t* _drop(const char* b, const jsmntok_t* t) {
    if (t->type == JSMN_PRIMITIVE) return ++t;
    if (t->type == JSMN_STRING) return ++t;
    if (t->type == JSMN_OBJECT) {
        int size = t->size;
        ++t;
        for (int i = 0; i < size; ++i) t = _drop(b, ++t);
        return t;
    }
    if (t->type == JSMN_ARRAY) {
        int size = t->size;
        ++t;
        for (int i = 0; i < size; ++i) t = _drop(b, t);
        return t;
    }
    return NULL;
}

static bool _is_key(const char* buffer, const jsmntok_t* t, const char* key_str, uint8_t l) {
    return (t->type == JSMN_STRING) && (l == (t->end - t->start)) &&
           ((strncmp(key_str, buffer + t->start, l) == 0));
}

static const jsmntok_t* _read_string(char* buffer, const jsmntok_t* t, const char** out) {
    if (t->type != JSMN_STRING) return NULL;
    *(buffer + t->end) = '\0';
    *out = buffer + t->start;
    return ++t;
}

static const jsmntok_t* _read_int(char* buffer, const jsmntok_t* t, uint16_t* out) {
    if (t->type != JSMN_PRIMITIVE) return NULL;
    char c = *(buffer + t->start);
    bool is_num = (c >= '0' && c <= '9') || (c == '-');
    if (!is_num) return NULL;
    *(buffer + t->end) = '\0';
    *out = atoi(buffer + t->start);
    return ++t;
}

static const jsmntok_t* _parse_meta_size(char* buffer, const jsmntok_t* t, pixi_atlas* atlas) {
    if (t->type != JSMN_OBJECT) return NULL;
    if (t->size < 2) return NULL;
    int size = t->size;
    ++t;

    for (int i = 0; i < size; ++i) {
        if (_is_key(buffer, t, "w", 1)) {
            t = _read_int(buffer, ++t, &atlas->image_w);
        } else if (_is_key(buffer, t, "h", 1)) {
            t = _read_int(buffer, ++t, &atlas->image_h);
        } else {
            t = _drop(buffer, ++t);
        }
        if (!t) return NULL;
    }
    return t;
}

static const jsmntok_t* _parse_meta(char* buffer, const jsmntok_t* t, pixi_atlas* atlas) {
    if (t->type != JSMN_OBJECT) return NULL;
    if (t->size < 2) return NULL;
    int size = t->size;
    ++t;

    for (int i = 0; i < size; ++i) {
        if (_is_key(buffer, t, "image", 5)) {
            t = _read_string(buffer, ++t, &atlas->image);
        } else if (_is_key(buffer, t, "size", 4)) {
            t = _parse_meta_size(buffer, ++t, atlas);
        } else {
            t = _drop(buffer, ++t);
        }
        if (!t) return NULL;
    }
    return t;
}

static const jsmntok_t* _parse_frame(char* buffer, const jsmntok_t* t, pixi_atlas_frame* frame) {
    if (t->type != JSMN_OBJECT) return NULL;
    if (t->size < 1) return NULL;
    int size = t->size;
    ++t;

    for (int i = 0; i < size; ++i) {
        if (_is_key(buffer, t, "frame", 5)) {
            ++t;
            if (t->type != JSMN_OBJECT) return NULL;
            if (t->size < 1) return NULL;
            int size2 = t->size;
            ++t;

            for (int j = 0; j < size2; ++j) {
                if (_is_key(buffer, t, "x", 1))
                    t = _read_int(buffer, ++t, &frame->x);
                else if (_is_key(buffer, t, "y", 1))
                    t = _read_int(buffer, ++t, &frame->y);
                else if (_is_key(buffer, t, "w", 1))
                    t = _read_int(buffer, ++t, &frame->w);
                else if (_is_key(buffer, t, "h", 1))
                    t = _read_int(buffer, ++t, &frame->h);
                else {
                    // skip key + drop value
                    t = _drop(buffer, ++t);
                }
                if (!t) return NULL;
            }
        } else {
            t = _drop(buffer, ++t);
        }
        if (!t) return NULL;
    }
    return t;
}

static const jsmntok_t* _parse_frames(char* buffer, const jsmntok_t* t, pixi_atlas* atlas) {
    if (t->type != JSMN_OBJECT) return NULL;
    if (t->size < 1) return NULL;
    int size = t->size;
    ++t;

    atlas->frames_size = size;
    atlas->frames = calloc(size, sizeof(pixi_atlas_frame));

    for (int i = 0; i < size; ++i) {
        pixi_atlas_frame* frame = &(atlas->frames[i]);
        t = _read_string(buffer, t, &frame->name);
        if (!t) {
            free(atlas->frames);
            return NULL;
        }

        t = _parse_frame(buffer, t, frame);
        if (!t) {
            free(atlas->frames);
            return NULL;
        }
    }
    return t;
}

static pixi_atlas* _parse_root(char* buffer, const jsmntok_t* tokens) {
    if (tokens[0].type != JSMN_OBJECT) return NULL;
    if (tokens[0].size < 2) return NULL;

    pixi_atlas atlas;

    const jsmntok_t* t = &tokens[1];
    for (int i = 0; i < tokens[0].size; ++i) {
        if (_is_key(buffer, t, "meta", 4)) {
            t = _parse_meta(buffer, ++t, &atlas);
        } else if (_is_key(buffer, t, "frames", 6)) {
            t = _parse_frames(buffer, ++t, &atlas);
        } else {
            t = _drop(buffer, ++t);
        }
        if (!t) return NULL;
    }

    // check all loaded/correct
    bool correct = atlas.image && atlas.image_w && atlas.image_h && atlas.frames_size;
    if (!correct) {
        if (atlas.frames) free(atlas.frames);
        return NULL;
    }

    // count all string size
    size_t l = strlen(atlas.image) + 1;
    for (int i = 0; i < atlas.frames_size; ++i) l += strlen(atlas.frames[i].name) + 1;

    // buffer for struct + all strings
    pixi_atlas* a = malloc(sizeof(pixi_atlas) + l);
    char* dst = ((char*)a) + sizeof(pixi_atlas);

    *a = atlas;

    // copy all strings
    const char* src = atlas.image;
    a->image = dst;
    while (*src) *dst++ = *src++;
    *dst++ = '\0';

    for (int i = 0; i < atlas.frames_size; ++i) {
        src = atlas.frames[i].name;
        a->frames[i].name = dst;
        while (*src) *dst++ = *src++;
        *dst++ = '\0';
    }

    return a;
}

static pixi_atlas* _parse_buffer(char* buffer, const int len) {
    jsmn_parser p;
    jsmn_init(&p);

    int token_count = jsmn_parse(&p, buffer, len, NULL, 0);
    if (token_count == JSMN_ERROR_NOMEM) {
        printf("Error: JSMN_ERROR_NOMEM - Not enough tokens were provided\n");
        return NULL;
    } else if (token_count == JSMN_ERROR_INVAL) {
        printf("Error: JSMN_ERROR_INVAL - Invalid character inside JSON string\n");
        return NULL;
    } else if (token_count == JSMN_ERROR_PART) {
        printf(
            "Error: JSMN_ERROR_PART - The string is not a full JSON packet, more bytes expected\n");
        return NULL;
    }

    jsmn_init(&p);
    jsmntok_t tokens[token_count];
    int rc = jsmn_parse(&p, buffer, len, tokens, token_count);
    assert(rc == token_count);

    return _parse_root(buffer, tokens);
}

pixi_atlas* pixi_atlas_parse(const char* filename) {
    struct stat s;
    int rc = stat(filename, &s);
    if (rc != 0) {
        perror("stat");
        return NULL;
    }

    size_t file_size = s.st_size;
    FILE* f = fopen(filename, "rt");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    char buffer[file_size];
    size_t rc2 = fread(buffer, 1, file_size, f);
    fclose(f);
    if (rc2 != file_size) return NULL;

    return _parse_buffer(buffer, file_size);
}

void pixi_atlas_free(pixi_atlas* atlas) {
    if (!atlas) return;
    if (atlas->frames) free(atlas->frames);
    free(atlas);
}

void pixi_atlas_dump(const pixi_atlas* atlas) {
    printf(">>> dump >>>\n");

    if (!atlas) {
        printf("atlas is NULL\n");
        return;
    }

    printf("meta image: %s [%dx%d] contain %d frames\n", atlas->image, atlas->image_w,
           atlas->image_h, atlas->frames_size);

    for (int i = 0; i < atlas->frames_size; ++i) {
        const pixi_atlas_frame* f = &atlas->frames[i];
        printf("- %03d %30s x:%4d; y:%4d; w:%4d; h:%4d\n", i + 1, f->name, f->x, f->y, f->w, f->h);
    }

    printf("<<< dump <<<\n");
}
