#include "render/draw_front.h"

#include "core/config.h"
#include "game/tuning.h"
#include "render/draw_game.h"
#include "render/draw_ui.h"

#include <math.h>
#include <stdio.h>

static const Color COL_TEXT = {236, 236, 244, 255};
static const Color COL_TEXT_DIM = {150, 154, 176, 255};
static const Color COL_TITLE = {255, 214, 90, 255};
static const Color COL_BORDER = {96, 102, 140, 255};
static const Color COL_SELECTED = {255, 224, 80, 255};
static const Color COL_ERROR = {240, 96, 80, 255};
static const Color COL_CRATE_RED = {208, 64, 56, 255};
static const Color COL_CRATE_BLUE = {56, 112, 226, 255};
static const Color COL_WOOD = {142, 98, 52, 255};
static const Color COL_WOOD_DARK = {92, 60, 30, 255};

static void sky(Render *r) {
    const int bands = 12;
    const f32 band_h = (f32)LOGICAL_H / (f32)bands;
    for (int i = 0; i < bands; i++) {
        const f32 t = (f32)i / (f32)(bands - 1);
        const Color c = {(u8)lerpf(26.0f, 92.0f, t), (u8)lerpf(30.0f, 64.0f, t), (u8)lerpf(60.0f, 104.0f, t), 255};
        render_fill_rect(r, (Rect){0.0f, (f32)i * band_h, (f32)LOGICAL_W, band_h + 1.0f}, c);
    }
}

static void crate_icon(Render *r, f32 x, f32 y, f32 s, bool blue) {
    render_fill_rect(r, (Rect){x, y, s, s}, COL_WOOD);
    render_fill_rect(r, (Rect){x + 2.0f, y + 2.0f, s - 4.0f, s - 4.0f}, blue ? COL_CRATE_BLUE : COL_CRATE_RED);
    render_line(r, vec2(x + 2.0f, y + 2.0f), vec2(x + s - 3.0f, y + s - 3.0f), COL_WOOD_DARK);
    render_line(r, vec2(x + s - 3.0f, y + 2.0f), vec2(x + 2.0f, y + s - 3.0f), COL_WOOD_DARK);
}

// Crates floating down under their parachutes, behind the title.
static void falling_crates(Render *r, u32 ticks) {
    for (int i = 0; i < 9; i++) {
        const f32 x = 30.0f + (f32)((i * 151) % 590);
        const f32 period = 360.0f + (f32)(i * 47 % 200);
        const f32 y = fmodf((f32)ticks * 0.6f + (f32)(i * 97), period + 60.0f) - 40.0f;
        if (y > 300.0f) {
            continue;
        }
        render_fill_rect(r, (Rect){x - 2.0f, y - 12.0f, 20.0f, 5.0f}, (Color){240, 236, 220, 255});
        render_line(r, vec2(x - 1.0f, y - 7.0f), vec2(x + 2.0f, y), (Color){240, 236, 220, 255});
        render_line(r, vec2(x + 16.0f, y - 7.0f), vec2(x + 13.0f, y), (Color){240, 236, 220, 255});
        crate_icon(r, x, y, 16.0f, i % 3 == 1);
    }
}

static void ground(Render *r) {
    render_fill_rect(r, (Rect){0.0f, 318.0f, (f32)LOGICAL_W, 42.0f}, (Color){74, 62, 92, 255});
    render_fill_rect(r, (Rect){0.0f, 318.0f, (f32)LOGICAL_W, 3.0f}, (Color){118, 160, 90, 255});
}

static void dim(Render *r, u8 alpha) {
    render_fill_rect(r, (Rect){0.0f, 0.0f, (f32)LOGICAL_W, (f32)LOGICAL_H}, (Color){8, 8, 16, alpha});
}

static void menu_entry(Render *r, const Front *f, i32 index, const char *label) {
    const Rect box = front_menu_rect(f, index);
    const bool selected = index == f->selected;
    ui_panel(r, box, selected ? COL_SELECTED : COL_BORDER);
    const f32 cx = box.x + box.w * 0.5f, ty = box.y + 6.0f;
    ui_text_shadow(r, cx, ty, label, selected ? COL_SELECTED : COL_TEXT, 1, ALIGN_CENTER);
    if (selected) {
        ui_text(r, box.x + 6.0f, ty, ">", COL_SELECTED, 1, ALIGN_LEFT);
        ui_text(r, box.x + box.w - 11.0f, ty, "<", COL_SELECTED, 1, ALIGN_LEFT);
    }
}

static void controls(Render *r, f32 y) {
    ui_text_shadow(r, (f32)LOGICAL_W * 0.5f, y, "BOUGER : ZQSD OU FLÈCHES   SAUTER : ESPACE   TIRER : X OU CLIC", COL_TEXT_DIM, 1,
                   ALIGN_CENTER);
    ui_text_shadow(r, (f32)LOGICAL_W * 0.5f, y + 13.0f, "CAISSE : C   CONSTRUIRE : TAB + ENTRÉE   SOLDAT : TAB   ORDRE : A   PAUSE : ÉCHAP",
                   COL_TEXT_DIM, 1, ALIGN_CENTER);
}

// ---- Screens -----------------------------------------------------------------------------------

static void draw_title(Render *r, const Front *f) {
    sky(r);
    falling_crates(r, f->screen_ticks);
    ground(r);
    const f32 cx = (f32)LOGICAL_W * 0.5f;
    const f32 bob = roundf(sinf((f32)f->screen_ticks * 0.05f) * 2.0f);
    ui_text_shadow(r, cx + 2.0f, 70.0f + bob + 2.0f, "CRATE SQUAD", (Color){90, 50, 30, 255}, 6, ALIGN_CENTER);
    ui_text(r, cx, 70.0f + bob, "CRATE SQUAD", COL_TITLE, 6, ALIGN_CENTER);
    ui_text_shadow(r, cx, 128.0f, "LA GUERRE DES CAISSES", COL_TEXT, 2, ALIGN_CENTER);
    for (int i = 0; i < 4; i++) {
        crate_icon(r, cx - 46.0f + (f32)i * 24.0f, 162.0f + (i == 1 || i == 2 ? 0.0f : 8.0f), 20.0f, i == 2);
    }
    menu_entry(r, f, TITLE_PLAY, "JOUER");
    menu_entry(r, f, TITLE_QUIT, "QUITTER");
    ui_text_shadow(r, cx, 330.0f, "HAUT / BAS : CHOISIR    ENTRÉE : VALIDER", COL_TEXT, 1, ALIGN_CENTER);
    ui_text(r, (f32)LOGICAL_W - 6.0f, 348.0f, "M6", COL_TEXT_DIM, 1, ALIGN_RIGHT);
}

// The level of the setup screen, one or two pixels per tile.
static void minimap(Render *r, const Level *lv, Rect area) {
    const f32 px = floorf(MIN(area.w / (f32)lv->width, area.h / (f32)lv->height));
    const f32 s = MAX(px, 1.0f);
    const f32 w = s * (f32)lv->width, h = s * (f32)lv->height;
    const f32 x0 = roundf(area.x + (area.w - w) * 0.5f), y0 = roundf(area.y + (area.h - h) * 0.5f);
    render_fill_rect(r, (Rect){x0 - 2.0f, y0 - 2.0f, w + 4.0f, h + 4.0f}, (Color){20, 22, 40, 255});
    for (i32 ty = 0; ty < lv->height; ty++) {
        for (i32 tx = 0; tx < lv->width; tx++) {
            Color c;
            switch (level_tile(lv, tx, ty)) {
            case TILE_SOLID:
                c = (Color){118, 104, 142, 255};
                break;
            case TILE_PLATFORM:
                c = (Color){176, 134, 72, 255};
                break;
            case TILE_LADDER:
                c = (Color){156, 116, 64, 255};
                break;
            case TILE_HAZARD:
                c = (Color){46, 110, 210, 255};
                break;
            default:
                continue;
            }
            render_fill_rect(r, (Rect){x0 + (f32)tx * s, y0 + (f32)ty * s, s, s}, c);
        }
    }
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        if (lv->has_spawn[t] && lv->pads[t].present) {
            const Vec2 sp = lv->spawns[t];
            render_fill_rect(r, (Rect){x0 + sp.x / (f32)TILE_SIZE * s - 2.0f, y0 + sp.y / (f32)TILE_SIZE * s - 5.0f, 4.0f, 5.0f},
                             ui_team_color(t));
        }
    }
    for (i32 z = 0; z < lv->drop_zone_count; z++) {
        const Vec2 d = lv->drop_zones[z];
        const bool blue = lv->drop_cfg[z].color == DROP_BLUE;
        render_fill_rect(r, (Rect){x0 + d.x / (f32)TILE_SIZE * s - 1.0f, y0 + d.y / (f32)TILE_SIZE * s, 3.0f, 3.0f},
                         blue ? COL_CRATE_BLUE : COL_CRATE_RED);
    }
}

static void setting_entry(Render *r, const Front *f, i32 index, const char *name, const char *value) {
    const Rect box = front_menu_rect(f, index);
    const bool selected = index == f->selected;
    ui_panel(r, box, selected ? COL_SELECTED : COL_BORDER);
    ui_text_shadow(r, box.x + 10.0f, box.y + 6.0f, name, COL_TEXT_DIM, 1, ALIGN_LEFT);
    char line[96];
    snprintf(line, sizeof(line), selected ? "< %s >" : "%s", value);
    ui_text_shadow(r, box.x + box.w - 10.0f, box.y + 6.0f, line, selected ? COL_SELECTED : COL_TEXT, 1, ALIGN_RIGHT);
}

static void draw_setup(Render *r, const Front *f) {
    sky(r);
    ground(r);
    const f32 cx = (f32)LOGICAL_W * 0.5f;
    ui_text_shadow(r, cx, 18.0f, "NOUVELLE PARTIE", COL_TITLE, 3, ALIGN_CENTER);
    const Rect area = {cx - 150.0f, 52.0f, 300.0f, 120.0f};
    ui_panel(r, (Rect){area.x - 6.0f, area.y - 6.0f, area.w + 12.0f, area.h + 26.0f}, COL_BORDER);
    if (f->preview_ok) {
        minimap(r, &f->preview, area);
        ui_text_shadow(r, cx, area.y + area.h + 6.0f, f->preview.name, COL_TEXT_DIM, 1, ALIGN_CENTER);
    } else {
        ui_text_shadow(r, cx, area.y + area.h * 0.5f, "NIVEAU INTROUVABLE", COL_ERROR, 1, ALIGN_CENTER);
    }
    setting_entry(r, f, SETUP_LEVEL, "NIVEAU", FRONT_LEVELS[f->level_index].title);
    setting_entry(r, f, SETUP_MODE, "MODE", f->spectate ? "IA CONTRE IA (SPECTATEUR)" : "JOUEUR CONTRE L'IA");
    menu_entry(r, f, SETUP_START, "LANCER LA PARTIE");
    menu_entry(r, f, SETUP_BACK, "RETOUR");
    if (f->error[0]) {
        ui_text_shadow(r, cx, 308.0f, "IMPOSSIBLE DE LANCER LA PARTIE", COL_ERROR, 1, ALIGN_CENTER);
    }
    controls(r, 326.0f);
}

static void draw_pause(Render *r, const Front *f) {
    dim(r, 170);
    ui_text_shadow(r, (f32)LOGICAL_W * 0.5f, 100.0f, "PAUSE", COL_TITLE, 4, ALIGN_CENTER);
    menu_entry(r, f, PAUSE_RESUME, "REPRENDRE");
    menu_entry(r, f, PAUSE_RESTART, "RECOMMENCER");
    menu_entry(r, f, PAUSE_QUIT, "ABANDONNER");
    controls(r, 300.0f);
}

static void headline(const GameState *gs, char *out, usize cap, Color *color) {
    const bool human = !gs->teams[0].ai;
    if (gs->match.winner < 0) {
        snprintf(out, cap, "MATCH NUL");
        *color = COL_TEXT;
    } else if (human && gs->match.winner == 0) {
        snprintf(out, cap, "VICTOIRE !");
        *color = COL_SELECTED;
    } else if (human) {
        snprintf(out, cap, "DÉFAITE");
        *color = COL_ERROR;
    } else {
        snprintf(out, cap, "LES %s GAGNENT", ui_team_name(gs->match.winner));
        *color = ui_team_color(gs->match.winner);
    }
}

static void draw_results(Render *r, const Front *f) {
    const GameState *gs = &f->sim.gs;
    dim(r, 190);
    const f32 cx = (f32)LOGICAL_W * 0.5f;
    char line[96];
    Color color;
    headline(gs, line, sizeof(line), &color);
    ui_text_shadow(r, cx, 40.0f, line, color, 4, ALIGN_CENTER);
    const u64 s = gs->match.end_tick / TICK_HZ;
    snprintf(line, sizeof(line), "DURÉE %02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
    ui_text_shadow(r, cx, 84.0f, line, COL_TEXT_DIM, 1, ALIGN_CENTER);

    const Rect table = {cx - 230.0f, 104.0f, 460.0f, 180.0f};
    ui_panel(r, table, COL_BORDER);
    static const char *const HEADERS[] = {"ÉQUIPE", "FABRIQUÉS", "ÉLIMINÉS", "PERTES", "SOLDATS", "BASE"};
    const f32 cols[] = {table.x + 14.0f, table.x + 160.0f, table.x + 230.0f, table.x + 300.0f, table.x + 365.0f, table.x + 430.0f};
    for (int c = 0; c < 6; c++) {
        ui_text_shadow(r, cols[c], table.y + 12.0f, HEADERS[c], COL_TEXT_DIM, 1, c == 0 ? ALIGN_LEFT : ALIGN_CENTER);
    }
    f32 y = table.y + 34.0f;
    for (i32 t = 0; t < MAX_TEAMS; t++) {
        const Team *team = &gs->teams[t];
        if (!team->playing) {
            continue;
        }
        render_fill_rect(r, (Rect){cols[0], y - 1.0f, 8.0f, 9.0f}, ui_team_color(t));
        snprintf(line, sizeof(line), "%s%s", ui_team_name(t), t == 0 && !team->ai ? " (VOUS)" : "");
        ui_text_shadow(r, cols[0] + 14.0f, y, line, t == gs->match.winner ? COL_SELECTED : COL_TEXT, 1, ALIGN_LEFT);
        const u32 values[] = {team->stats.built, team->stats.kills, team->stats.losses, team_soldier_count(gs, t)};
        for (int c = 0; c < 4; c++) {
            snprintf(line, sizeof(line), "%u", values[c]);
            ui_text_shadow(r, cols[c + 1], y, line, COL_TEXT, 1, ALIGN_CENTER);
        }
        if (!team->has_base) {
            snprintf(line, sizeof(line), "-");
        } else if (team->base_hp <= 0) {
            snprintf(line, sizeof(line), "DÉTRUITE");
        } else {
            snprintf(line, sizeof(line), "%d%%", (team->base_hp * 100 + TUNING_BASE_HP - 1) / TUNING_BASE_HP);
        }
        ui_text_shadow(r, cols[5], y, line, team->base_hp <= 0 && team->has_base ? COL_ERROR : COL_TEXT, 1, ALIGN_CENTER);
        y += 22.0f;
    }
    menu_entry(r, f, RESULTS_AGAIN, "REJOUER");
    menu_entry(r, f, RESULTS_MENU, "MENU PRINCIPAL");
}

void draw_front(Render *r, const Front *f, const Vec2 *crosshair, f32 alpha, u32 flags) {
    const Sim *sim = front_sim_const(f);
    switch (f->screen) {
    case SCREEN_TITLE:
    case SCREEN_QUIT:
        draw_title(r, f);
        break;
    case SCREEN_SETUP:
        draw_setup(r, f);
        break;
    case SCREEN_PLAY:
        draw_game(r, &sim->gs, &sim->cam, &sim->menu, &sim->fx, crosshair, alpha, flags);
        break;
    case SCREEN_PAUSE:
        draw_game(r, &sim->gs, &sim->cam, &sim->menu, &sim->fx, NULL, 1.0f, flags);
        draw_pause(r, f);
        break;
    case SCREEN_RESULTS:
        draw_game(r, &sim->gs, &sim->cam, NULL, &sim->fx, NULL, 1.0f, flags & ~(u32)DRAW_NAV);
        draw_results(r, f);
        break;
    }
}
