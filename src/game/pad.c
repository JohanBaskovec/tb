#include "game/pad.h"

#include "core/log.h"
#include "game/crate.h"
#include "game/loot.h"
#include "game/tuning.h"

#include <string.h>

// ---- Recipes (data-driven: change a product here, nothing else) ---------------------------------

typedef struct Recipe {
    PadShape shape;
    PadProduct red, blue;
    u32 size;
    const char *name;
} Recipe;

static const Recipe RECIPES[SHAPE_COUNT] = {
    {SHAPE_1X1, PRODUCT_PISTOL, PRODUCT_UZI, 1, "1x1"},
    {SHAPE_2X1_H, PRODUCT_GRENADES, PRODUCT_STRONG_GRENADES, 2, "2x1 horizontal"},
    {SHAPE_2X1_V, PRODUCT_COMPANION, PRODUCT_STRONG_COMPANION, 2, "2x1 vertical"},
    {SHAPE_L, PRODUCT_UZI, PRODUCT_ROCKET_LAUNCHER, 3, "L"},
    {SHAPE_2X2, PRODUCT_HEAVY_COMPANION, PRODUCT_HEAVY_ARMORED_COMPANION, 4, "2x2"},
    {SHAPE_4X2, PRODUCT_TANK, PRODUCT_HEAVY_TANK, 8, "4x2"},
};

static const char *const PRODUCT_NAMES[PRODUCT_COUNT] = {
    "pistol",          "uzi",           "grenades",
    "strong grenades", "companion",     "strong companion",
    "rocket launcher", "heavy companion", "heavy armored companion",
    "tank",            "heavy tank",
};

PadProduct pad_recipe(PadShape shape, bool blue) {
    ASSERT(shape >= 0 && shape < SHAPE_COUNT);
    return blue ? RECIPES[shape].blue : RECIPES[shape].red;
}

u32 pad_shape_size(PadShape shape) {
    ASSERT(shape >= 0 && shape < SHAPE_COUNT);
    return RECIPES[shape].size;
}

const char *pad_shape_name(PadShape shape) {
    return (shape >= 0 && shape < SHAPE_COUNT) ? RECIPES[shape].name : "?";
}

const char *pad_product_name(PadProduct product) {
    return (product >= 0 && product < PRODUCT_COUNT) ? PRODUCT_NAMES[product] : "?";
}

// ---- Shapes ------------------------------------------------------------------------------------

typedef struct Placement {
    u32 n;
    PadCoord c[PAD_CELLS]; // topmost row first, then left to right
} Placement;

#define MAX_PLACEMENTS 8

static void placement_add(Placement *p, i32 col, i32 row) {
    // Insertion sort keeps the cells ordered top first, then left to right.
    u32 i = p->n++;
    while (i > 0 && (p->c[i - 1].row > row || (p->c[i - 1].row == row && p->c[i - 1].col > col))) {
        p->c[i] = p->c[i - 1];
        i--;
    }
    p->c[i] = (PadCoord){(u8)col, (u8)row};
}

static Placement make(u32 count, const i32 *colrow) {
    Placement p = {0};
    for (u32 i = 0; i < count; i++) {
        placement_add(&p, colrow[i * 2], colrow[i * 2 + 1]);
    }
    return p;
}

// Every position of `shape` inside the 4x2 grid. Returns the number of placements.
static u32 shape_placements(PadShape shape, Placement out[MAX_PLACEMENTS]) {
    u32 n = 0;
    const i32 bottom = PAD_ROWS - 1;
    switch (shape) {
    case SHAPE_1X1:
        for (i32 r = 0; r < PAD_ROWS; r++) {
            for (i32 c = 0; c < PAD_COLS; c++) {
                out[n++] = make(1, (i32[]){c, r});
            }
        }
        break;
    case SHAPE_2X1_H:
        for (i32 r = 0; r < PAD_ROWS; r++) {
            for (i32 c = 0; c + 1 < PAD_COLS; c++) {
                out[n++] = make(2, (i32[]){c, r, c + 1, r});
            }
        }
        break;
    case SHAPE_2X1_V:
        for (i32 c = 0; c < PAD_COLS; c++) {
            out[n++] = make(2, (i32[]){c, 0, c, bottom});
        }
        break;
    case SHAPE_L:
        for (i32 c = 0; c + 1 < PAD_COLS; c++) {
            out[n++] = make(3, (i32[]){c, bottom, c + 1, bottom, c, 0});     // the top one on the left end
            out[n++] = make(3, (i32[]){c, bottom, c + 1, bottom, c + 1, 0}); // ...or on the right end
        }
        break;
    case SHAPE_2X2:
        for (i32 c = 0; c + 1 < PAD_COLS; c++) {
            out[n++] = make(4, (i32[]){c, 0, c + 1, 0, c, bottom, c + 1, bottom});
        }
        break;
    case SHAPE_4X2: {
        Placement all = {0};
        for (i32 r = 0; r < PAD_ROWS; r++) {
            for (i32 c = 0; c < PAD_COLS; c++) {
                placement_add(&all, c, r);
            }
        }
        out[n++] = all;
        break;
    }
    default:
        break;
    }
    return n;
}

// ---- pad_options -------------------------------------------------------------------------------

typedef struct Fit {
    bool found;
    Placement placement;
} Fit;

// Is `candidate` a better pick than `best`: higher up first, then further left.
static bool placement_before(const Placement *candidate, const Placement *best) {
    for (u32 i = 0; i < candidate->n; i++) {
        const PadCoord a = candidate->c[i], b = best->c[i];
        if (a.row != b.row) {
            return a.row < b.row;
        }
        if (a.col != b.col) {
            return a.col < b.col;
        }
    }
    return false;
}

void pad_options(const PadGrid *grid, PadOptionList *out) {
    out->count = 0;
    for (i32 shape = SHAPE_COUNT - 1; shape >= 0; shape--) { // biggest first
        Placement placements[MAX_PLACEMENTS];
        const u32 np = shape_placements((PadShape)shape, placements);
        Fit red = {0}, blue = {0}; // best all-red placement, best placement with at least one blue crate
        for (u32 i = 0; i < np; i++) {
            const Placement *pl = &placements[i];
            bool full = true, has_blue = false;
            for (u32 k = 0; k < pl->n && full; k++) {
                const u8 cell = grid->cells[pl->c[k].row][pl->c[k].col];
                full = cell != PAD_EMPTY;
                has_blue = has_blue || cell == PAD_BLUE;
            }
            if (!full) {
                continue;
            }
            Fit *fit = has_blue ? &blue : &red;
            if (!fit->found || placement_before(pl, &fit->placement)) {
                fit->found = true;
                fit->placement = *pl;
            }
        }
        for (int v = 0; v < 2; v++) { // red version first
            const Fit *fit = v == 0 ? &red : &blue;
            if (!fit->found) {
                continue;
            }
            PadOption *o = &out->items[out->count++];
            o->shape = (PadShape)shape;
            o->blue = v == 1;
            o->product = pad_recipe(o->shape, o->blue);
            o->count = fit->placement.n;
            for (u32 k = 0; k < fit->placement.n; k++) {
                o->cells[k] = fit->placement.c[k];
            }
        }
    }
}

// ---- World -------------------------------------------------------------------------------------

static const PadSite *pad_site(const GameState *gs, i32 team) {
    if (team < 0 || team >= MAX_TEAMS || !gs->level->pads[team].present) {
        return NULL;
    }
    return &gs->level->pads[team];
}

Rect pad_rect(const GameState *gs, i32 team) {
    const PadSite *site = pad_site(gs, team);
    if (!site) {
        return (Rect){0, 0, 0, 0};
    }
    const i32 top = site->cell_y - (PAD_ROWS - 1);
    return (Rect){(f32)(site->cell_x * TILE_SIZE), (f32)(top * TILE_SIZE), (f32)(PAD_COLS * TILE_SIZE),
                  (f32)(PAD_ROWS * TILE_SIZE)};
}

bool pad_usable(const GameState *gs, i32 team) {
    return pad_site(gs, team) != NULL; // M3: and the team's base is still standing
}

bool pad_read_grid(const GameState *gs, i32 team, PadGrid *grid, EntityId ids[PAD_ROWS][PAD_COLS]) {
    *grid = (PadGrid){0};
    if (ids) {
        for (i32 r = 0; r < PAD_ROWS; r++) {
            for (i32 c = 0; c < PAD_COLS; c++) {
                ids[r][c] = ENTITY_NONE;
            }
        }
    }
    const PadSite *site = pad_site(gs, team);
    if (!site) {
        return false;
    }
    const i32 top = site->cell_y - (PAD_ROWS - 1);
    for (u16 i = 1; i <= MAX_ENTITIES; i++) {
        const Entity *e = &gs->entities.slots[i];
        if (e->type != ENT_CRATE || e->pending_destroy || e->as.crate.state != CRATE_RESTING) {
            continue;
        }
        const i32 col = e->as.crate.cell_x - site->cell_x;
        const i32 row = e->as.crate.cell_y - top;
        if (col < 0 || col >= PAD_COLS || row < 0 || row >= PAD_ROWS) {
            continue;
        }
        grid->cells[row][col] = e->as.crate.kind == CRATE_BLUE ? PAD_BLUE : PAD_RED;
        if (ids) {
            ids[row][col] = (EntityId){i, e->generation};
        }
    }
    return true;
}

void pad_world_options(const GameState *gs, i32 team, PadOptionList *out) {
    out->count = 0;
    PadGrid grid;
    if (pad_usable(gs, team) && pad_read_grid(gs, team, &grid, NULL)) {
        pad_options(&grid, out);
    }
}

// ---- Building ----------------------------------------------------------------------------------

#define PRODUCT_SIZE TUNING_LOOT_W

// Where the product appears: the column just left of the pad (the menu is drawn on its right), else
// just right of it, else above it.
static bool product_spawn_pos(const GameState *gs, const PadSite *site, Vec2 *out) {
    const Level *lv = gs->level;
    const i32 top = site->cell_y - (PAD_ROWS - 1);
    const i32 cols[3] = {site->cell_x - 1, site->cell_x + PAD_COLS, site->cell_x + PAD_COLS / 2};
    const i32 rows[3] = {top, top, top - 1};
    for (int i = 0; i < 3; i++) {
        if (!level_is_solid(lv, cols[i], rows[i]) && !level_is_solid(lv, cols[i], rows[i] - 1)) {
            *out = vec2((f32)(cols[i] * TILE_SIZE) + (TUNING_CRATE_SIZE - PRODUCT_SIZE) * 0.5f,
                        (f32)(rows[i] * TILE_SIZE) + (TUNING_CRATE_SIZE - PRODUCT_SIZE));
            return true;
        }
    }
    return false;
}

bool pad_build(GameState *gs, i32 team, PadShape shape, bool blue) {
    if (!pad_usable(gs, team) || shape < 0 || shape >= SHAPE_COUNT) {
        return false;
    }
    PadGrid grid;
    EntityId ids[PAD_ROWS][PAD_COLS];
    pad_read_grid(gs, team, &grid, ids);
    PadOptionList options;
    pad_options(&grid, &options);
    const PadOption *chosen = NULL;
    for (u32 i = 0; i < options.count; i++) {
        if (options.items[i].shape == shape && options.items[i].blue == blue) {
            chosen = &options.items[i];
        }
    }
    if (!chosen) {
        return false;
    }
    const PadSite *site = pad_site(gs, team);
    Vec2 where;
    if (!product_spawn_pos(gs, site, &where)) {
        return false;
    }
    // Create the product before consuming anything, so a full pool changes nothing.
    EntityId pid;
    WeaponKind weapon;
    if (weapon_from_product(chosen->product, &weapon)) {
        pid = loot_spawn_weapon(gs, weapon, weapon_def(weapon)->max_ammo, where);
    } else {
        pid = pool_create(&gs->entities, ENT_PRODUCT);
        Entity *pe = pool_get(&gs->entities, pid);
        if (pe) {
            pe->body.size = vec2(PRODUCT_SIZE, PRODUCT_SIZE);
            pe->body.pos = where;
            pe->prev_pos = where;
            pe->as.product = (Product){.kind = chosen->product, .team = team};
        }
    }
    if (!entity_id_valid(pid)) {
        return false;
    }

    for (u32 k = 0; k < chosen->count; k++) { // top first
        crate_consume(gs, ids[chosen->cells[k].row][chosen->cells[k].col]);
    }
    gs->products_built++;
    const Rect r = pad_rect(gs, team);
    events_push(&gs->events, EV_PAD_BUILT, vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f), (i32)chosen->product, team);
    return true;
}

bool pad_debug_fill(GameState *gs, i32 team, const char *top, const char *bottom) {
    const PadSite *site = pad_site(gs, team);
    const char *rows[PAD_ROWS] = {top, bottom};
    if (!site) {
        return false;
    }
    for (i32 r = 0; r < PAD_ROWS; r++) {
        if (strlen(rows[r]) != PAD_COLS) {
            return false;
        }
        for (i32 c = 0; c < PAD_COLS; c++) {
            const char ch = rows[r][c];
            if (ch != '.' && ch != 'r' && ch != 'b') {
                return false;
            }
        }
    }
    const i32 top_row = site->cell_y - (PAD_ROWS - 1);
    for (i32 r = 0; r < PAD_ROWS; r++) {
        for (i32 c = 0; c < PAD_COLS; c++) {
            const char ch = rows[r][c];
            const i32 cx = site->cell_x + c, cy = top_row + r;
            if (ch == '.' || level_is_solid(gs->level, cx, cy)) {
                continue;
            }
            const Vec2 pos = vec2((f32)(cx * TILE_SIZE), (f32)(cy * TILE_SIZE));
            crate_spawn(gs, ch == 'b' ? CRATE_BLUE : CRATE_RED, pos, CRATE_RESTING, -1);
        }
    }
    return true;
}
