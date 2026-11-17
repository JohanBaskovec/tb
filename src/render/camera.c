#include "render/camera.h"

#define CAM_DEAD_HALF_W 24.0f
#define CAM_DEAD_HALF_H 20.0f
#define CAM_LOOK_AHEAD 28.0f
#define CAM_LOOK_SPEED 60.0f // px/s
#define CAM_SMOOTH 0.18f     // fraction of the remaining distance covered each tick

void camera_init(Camera *c, f32 view_w, f32 view_h) {
    c->pos = vec2(0, 0);
    c->prev_pos = c->pos;
    c->view_w = view_w;
    c->view_h = view_h;
    c->look_x = 0.0f;
}

static f32 clamp_axis(f32 pos, f32 view, f32 level) {
    if (level <= view) {
        return (level - view) * 0.5f; // smaller than the view: center it
    }
    return clampf(pos, 0.0f, level - view);
}

static Vec2 clamp_to_level(const Camera *c, Vec2 p, const Level *lv) {
    return vec2(clamp_axis(p.x, c->view_w, level_width_px(lv)),
                clamp_axis(p.y, c->view_h, level_height_px(lv)));
}

void camera_set(Camera *c, Vec2 top_left, const Level *lv) {
    c->pos = clamp_to_level(c, top_left, lv);
    c->prev_pos = c->pos;
}

void camera_follow(Camera *c, Vec2 target, i32 facing, const Level *lv, bool snap) {
    c->prev_pos = c->pos;
    c->look_x = approachf(c->look_x, (f32)facing * CAM_LOOK_AHEAD, CAM_LOOK_SPEED * TICK_DT);

    const f32 tx = target.x + c->look_x;
    const f32 ty = target.y;

    Vec2 goal;
    if (snap) {
        goal = vec2(tx - c->view_w * 0.5f, ty - c->view_h * 0.5f);
        c->pos = clamp_to_level(c, goal, lv);
        c->prev_pos = c->pos;
        return;
    }

    // Move only as far as needed to keep the target inside the dead zone.
    const f32 cx = c->pos.x + c->view_w * 0.5f;
    const f32 cy = c->pos.y + c->view_h * 0.5f;
    f32 gx = cx, gy = cy;
    if (tx > cx + CAM_DEAD_HALF_W) {
        gx = tx - CAM_DEAD_HALF_W;
    } else if (tx < cx - CAM_DEAD_HALF_W) {
        gx = tx + CAM_DEAD_HALF_W;
    }
    if (ty > cy + CAM_DEAD_HALF_H) {
        gy = ty - CAM_DEAD_HALF_H;
    } else if (ty < cy - CAM_DEAD_HALF_H) {
        gy = ty + CAM_DEAD_HALF_H;
    }
    goal = vec2(gx - c->view_w * 0.5f, gy - c->view_h * 0.5f);
    Vec2 next = vec2(lerpf(c->pos.x, goal.x, CAM_SMOOTH), lerpf(c->pos.y, goal.y, CAM_SMOOTH));
    c->pos = clamp_to_level(c, next, lv);
}

Vec2 camera_interpolated(const Camera *c, f32 alpha) {
    return vec2(lerpf(c->prev_pos.x, c->pos.x, alpha), lerpf(c->prev_pos.y, c->pos.y, alpha));
}
