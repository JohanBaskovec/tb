#include "game/level.h"

#include "core/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEVEL_FILE_BYTES (1 << 20)

static void set_err(char *err, usize err_len, const char *fmt, ...) CS_PRINTF_LIKE(3, 4);

static void set_err(char *err, usize err_len, const char *fmt, ...) {
    if (!err || err_len == 0) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(err, err_len, fmt, args);
    va_end(args);
}

// Returns the next line (without '\n' or '\r'). False at end of text.
static bool next_line(const char **p, const char **line, usize *len) {
    if (**p == '\0') {
        return false;
    }
    const char *s = *p;
    const char *e = s;
    while (*e != '\0' && *e != '\n') {
        e++;
    }
    *line = s;
    *len = (usize)(e - s);
    *p = (*e != '\0') ? e + 1 : e;
    if (*len > 0 && s[*len - 1] == '\r') {
        (*len)--;
    }
    return true;
}

static bool parse_header_line(Level *lv, const char *line, usize len, int line_no, char *err,
                              usize err_len) {
    const char *eq = (const char *)memchr(line, '=', len);
    if (!eq) {
        set_err(err, err_len, "line %d: expected key=value", line_no);
        return false;
    }
    usize klen = (usize)(eq - line);
    const char *val = eq + 1;
    usize vlen = len - klen - 1;
    if (klen == 4 && strncmp(line, "name", 4) == 0) {
        snprintf(lv->name, sizeof(lv->name), "%.*s", (int)vlen, val);
        return true;
    }
    set_err(err, err_len, "line %d: unknown header key '%.*s'", line_no, (int)klen, line);
    return false;
}

static bool fill_cell(Level *lv, i32 x, i32 y, char c, int line_no, char *err, usize err_len) {
    u8 *cell = &lv->tiles[y * lv->width + x];
    switch (c) {
    case '.':
        *cell = TILE_EMPTY;
        return true;
    case '#':
        *cell = TILE_SOLID;
        return true;
    case '=':
        *cell = TILE_PLATFORM;
        return true;
    case 'H':
        *cell = TILE_LADDER;
        return true;
    case '~':
        *cell = TILE_HAZARD;
        return true;
    case '1':
    case '2':
    case '3':
    case '4': {
        int team = c - '1';
        if (lv->has_spawn[team]) {
            set_err(err, err_len, "line %d: duplicate spawn '%c'", line_no, c);
            return false;
        }
        lv->has_spawn[team] = true;
        lv->spawns[team] = vec2(((f32)x + 0.5f) * (f32)TILE_SIZE, (f32)(y + 1) * (f32)TILE_SIZE);
        *cell = TILE_EMPTY;
        return true;
    }
    case 'D':
        if (lv->drop_zone_count >= MAX_DROP_ZONES) {
            set_err(err, err_len, "line %d: too many drop zones (max %d)", line_no,
                    MAX_DROP_ZONES);
            return false;
        }
        lv->drop_zones[lv->drop_zone_count++] =
            vec2(((f32)x + 0.5f) * (f32)TILE_SIZE, (f32)y * (f32)TILE_SIZE);
        *cell = TILE_EMPTY;
        return true;
    default:
        set_err(err, err_len, "line %d, column %d: unknown tile '%c'", line_no, (int)x + 1, c);
        return false;
    }
}

bool level_parse(Level *lv, Arena *arena, const char *text, char *err, usize err_len) {
    memset(lv, 0, sizeof(*lv));
    const char *p = text;
    const char *line;
    usize len;
    int line_no = 0;

    // Header.
    bool saw_separator = false;
    while (next_line(&p, &line, &len)) {
        line_no++;
        if (len == 3 && strncmp(line, "---", 3) == 0) {
            saw_separator = true;
            break;
        }
        if (len == 0 || line[0] == ';') {
            continue;
        }
        if (!parse_header_line(lv, line, len, line_no, err, err_len)) {
            return false;
        }
    }
    if (!saw_separator) {
        set_err(err, err_len, "missing '---' separator line");
        return false;
    }

    // Pass 1: measure the map and validate its shape.
    const char *map_start = p;
    const int map_first_line = line_no + 1;
    i32 width = 0, height = 0;
    bool pending_blank = false;
    while (next_line(&p, &line, &len)) {
        line_no++;
        if (len == 0) {
            pending_blank = true;
            continue;
        }
        if (pending_blank) {
            set_err(err, err_len, "line %d: blank line inside the map", line_no - 1);
            return false;
        }
        if (height == 0) {
            width = (i32)len;
        } else if ((i32)len != width) {
            set_err(err, err_len, "line %d: row has %d columns, expected %d", line_no, (int)len,
                    (int)width);
            return false;
        }
        height++;
    }
    if (width == 0 || height == 0) {
        set_err(err, err_len, "the map is empty");
        return false;
    }
    if (width > MAX_LEVEL_W || height > MAX_LEVEL_H) {
        set_err(err, err_len, "map is %dx%d, maximum is %dx%d", (int)width, (int)height,
                MAX_LEVEL_W, MAX_LEVEL_H);
        return false;
    }

    lv->width = width;
    lv->height = height;
    lv->tiles = ARENA_PUSH_ARRAY(arena, u8, (usize)(width * height));
    if (!lv->tiles) {
        set_err(err, err_len, "level arena is full");
        return false;
    }

    // Pass 2: fill.
    p = map_start;
    for (i32 y = 0; y < height; y++) {
        next_line(&p, &line, &len);
        for (i32 x = 0; x < width; x++) {
            if (!fill_cell(lv, x, y, line[x], map_first_line + (int)y, err, err_len)) {
                return false;
            }
        }
    }
    return true;
}

bool level_load_file(Level *lv, Arena *arena, const char *path, char *err, usize err_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        set_err(err, err_len, "cannot open '%s'", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > MAX_LEVEL_FILE_BYTES) {
        fclose(f);
        set_err(err, err_len, "'%s' has an invalid size", path);
        return false;
    }
    char *buf = (char *)malloc((usize)size + 1);
    if (!buf) {
        fclose(f);
        set_err(err, err_len, "out of memory");
        return false;
    }
    usize got = fread(buf, 1, (usize)size, f);
    fclose(f);
    buf[got] = '\0';
    bool ok = level_parse(lv, arena, buf, err, err_len);
    free(buf);
    return ok;
}

TileType level_tile(const Level *lv, i32 tx, i32 ty) {
    if (tx < 0 || tx >= lv->width || ty < 0) {
        return TILE_SOLID;
    }
    if (ty >= lv->height) {
        return TILE_HAZARD;
    }
    return (TileType)lv->tiles[ty * lv->width + tx];
}

bool level_is_solid(const Level *lv, i32 tx, i32 ty) {
    return level_tile(lv, tx, ty) == TILE_SOLID;
}

bool level_is_one_way(const Level *lv, i32 tx, i32 ty) {
    TileType t = level_tile(lv, tx, ty);
    if (t == TILE_PLATFORM) {
        return true;
    }
    return t == TILE_LADDER && level_tile(lv, tx, ty - 1) != TILE_LADDER;
}

TileType level_tile_at_px(const Level *lv, f32 x, f32 y) {
    return level_tile(lv, level_px_to_tile(x), level_px_to_tile(y));
}
