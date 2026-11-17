#include "render/render.h"

#include "core/config.h"
#include "core/log.h"
#include "core/png.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

struct Render {
    bool headless;
    SDL_Window *window;
    SDL_Surface *surface; // headless only
    SDL_Renderer *renderer;
};

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
    return r;
}

void render_destroy(Render *r) {
    if (!r) {
        return;
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
