#include "render/render.h"

#include "core/config.h"
#include "core/log.h"
#include "core/png.h"
#include "render/draw_ui.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

struct Render {
    bool headless;
    SDL_Window *window;
    SDL_Surface *surface; // headless only
    SDL_Renderer *renderer;
    SDL_Texture *sprites[SPRITE_PALETTE_COUNT]; // one atlas per palette
};

// Builds every sprite atlas (team colors from draw_ui) and uploads it.
static bool load_sprites(Render *r) {
    i32 w, h;
    sprites_atlas_size(&w, &h);
    u8 *rgba = (u8 *)malloc((usize)w * (usize)h * 4u);
    if (!rgba) {
        return false;
    }
    bool ok = true;
    for (i32 p = 0; ok && p < SPRITE_PALETTE_COUNT; p++) {
        const i32 team = p < MAX_TEAMS ? p : 0;
        const Color c = ui_team_color(team), d = ui_team_color_dark(team);
        sprites_build_atlas(rgba, (const u8[3]){c.r, c.g, c.b}, (const u8[3]){d.r, d.g, d.b}, p == SPRITE_PALETTE_FLASH);
        r->sprites[p] = SDL_CreateTexture(r->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        ok = r->sprites[p] && SDL_UpdateTexture(r->sprites[p], NULL, rgba, w * 4) &&
             SDL_SetTextureScaleMode(r->sprites[p], SDL_SCALEMODE_NEAREST) &&
             SDL_SetTextureBlendMode(r->sprites[p], SDL_BLENDMODE_BLEND);
    }
    free(rgba);
    if (!ok) {
        LOG_ERROR("sprite atlas: %s", SDL_GetError());
    }
    return ok;
}

Render *render_create(bool headless, const char *title) {
    Render *r = (Render *)calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    r->headless = headless;

    if (headless) {
        r->surface = SDL_CreateSurface(LOGICAL_W, LOGICAL_H, SDL_PIXELFORMAT_RGBA32);
        if (!r->surface) {
            LOG_ERROR("SDL_CreateSurface failed: %s", SDL_GetError());
            render_destroy(r);
            return NULL;
        }
        r->renderer = SDL_CreateSoftwareRenderer(r->surface);
    } else {
        if (!SDL_CreateWindowAndRenderer(title, LOGICAL_W * 2, LOGICAL_H * 2,
                                         SDL_WINDOW_RESIZABLE, &r->window, &r->renderer)) {
            LOG_ERROR("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
            render_destroy(r);
            return NULL;
        }
        SDL_SetRenderLogicalPresentation(r->renderer, LOGICAL_W, LOGICAL_H,
                                         SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        SDL_SetRenderVSync(r->renderer, 1);
    }
    if (!r->renderer) {
        LOG_ERROR("renderer creation failed: %s", SDL_GetError());
        render_destroy(r);
        return NULL;
    }
    SDL_SetRenderDrawBlendMode(r->renderer, SDL_BLENDMODE_BLEND); // colors with alpha < 255 are see-through
    if (!load_sprites(r)) {
        render_destroy(r);
        return NULL;
    }
    return r;
}

void render_destroy(Render *r) {
    if (!r) {
        return;
    }
    for (i32 p = 0; p < SPRITE_PALETTE_COUNT; p++) {
        if (r->sprites[p]) {
            SDL_DestroyTexture(r->sprites[p]);
        }
    }
    if (r->renderer) {
        SDL_DestroyRenderer(r->renderer);
    }
    if (r->window) {
        SDL_DestroyWindow(r->window);
    }
    if (r->surface) {
        SDL_DestroySurface(r->surface);
    }
    free(r);
}

static void set_color(Render *r, Color c) {
    SDL_SetRenderDrawColor(r->renderer, c.r, c.g, c.b, c.a);
}

void render_begin(Render *r, Color clear) {
    set_color(r, clear);
    SDL_RenderClear(r->renderer);
}

void render_end(Render *r) {
    SDL_RenderPresent(r->renderer);
}

void render_fill_rect(Render *r, Rect rect, Color c) {
    SDL_FRect fr = {rect.x, rect.y, rect.w, rect.h};
    set_color(r, c);
    SDL_RenderFillRect(r->renderer, &fr);
}

void render_outline_rect(Render *r, Rect rect, Color c) {
    SDL_FRect fr = {rect.x, rect.y, rect.w, rect.h};
    set_color(r, c);
    SDL_RenderRect(r->renderer, &fr);
}

void render_line(Render *r, Vec2 a, Vec2 b, Color c) {
    set_color(r, c);
    SDL_RenderLine(r->renderer, a.x, a.y, b.x, b.y);
}

void render_sprite(Render *r, const SpriteDraw *d) {
    if (d->id < 0 || d->id >= SPR_COUNT || d->palette < 0 || d->palette >= SPRITE_PALETTE_COUNT) {
        return;
    }
    SDL_Texture *tex = r->sprites[d->palette];
    const SpriteInfo info = sprite_info(d->id);
    const Rect src = sprite_rect(d->id);
    const f32 sx = d->scale_x > 0.0f ? d->scale_x : 1.0f, sy = d->scale_y > 0.0f ? d->scale_y : 1.0f;
    const Vec2 pivot = d->pivot_center ? vec2((f32)info.w * 0.5f, (f32)info.h * 0.5f) : info.pivot;
    const f32 px = (d->flip_x ? (f32)info.w - pivot.x : pivot.x) * sx;
    const f32 py = (d->flip_y ? (f32)info.h - pivot.y : pivot.y) * sy;
    f32 x = d->pos.x - px, y = d->pos.y - py;
    if (d->angle == 0.0f) { // stay on the pixel grid
        x = SDL_roundf(x);
        y = SDL_roundf(y);
    }
    const SDL_FRect s = {src.x, src.y, src.w, src.h};
    const SDL_FRect dst = {x, y, (f32)info.w * sx, (f32)info.h * sy};
    const SDL_FPoint center = {px, py};
    const SDL_FlipMode flip = d->flip_x ? SDL_FLIP_HORIZONTAL : (d->flip_y ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(tex, d->alpha ? d->alpha : 255);
    SDL_RenderTextureRotated(r->renderer, tex, &s, &dst, (double)d->angle, &center, flip);
}

Vec2 render_window_to_logical(const Render *r, f32 window_x, f32 window_y) {
    f32 x = window_x, y = window_y;
    if (r->renderer && !r->headless) {
        SDL_RenderCoordinatesFromWindow(r->renderer, window_x, window_y, &x, &y);
    }
    return vec2(x, y);
}

void render_set_title(Render *r, const char *title) {
    if (r->window) {
        SDL_SetWindowTitle(r->window, title);
    }
}

const char *render_title(const Render *r) {
    return r->window ? SDL_GetWindowTitle(r->window) : "";
}

bool render_screenshot(Render *r, const char *path) {
    SDL_Surface *raw = SDL_RenderReadPixels(r->renderer, NULL);
    if (!raw) {
        LOG_ERROR("SDL_RenderReadPixels failed: %s", SDL_GetError());
        return false;
    }
    SDL_Surface *rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(raw);
    if (!rgba) {
        LOG_ERROR("SDL_ConvertSurface failed: %s", SDL_GetError());
        return false;
    }
    bool ok = png_write_rgba(path, (const u8 *)rgba->pixels, (u32)rgba->w, (u32)rgba->h,
                             (usize)rgba->pitch);
    SDL_DestroySurface(rgba);
    return ok;
}
