#include "game/nav.h"

#include "core/log.h"
#include "game/tuning.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SIM_MAX_TICKS 150      // longest move simulated (a long fall)
#define SIM_STILL_TICKS 30     // a move that has not left its node by then leads nowhere
#define ROBUST_OFFSET 5.0f     // a move must end at the same node when started this far off the node's center
#define MAX_CANDIDATES 64
#define SIM_BAD_START (-2)     // the body cannot rest there (inside a wall): the start says nothing about the move
#define JUMP_PENALTY 8
#define DROP_PENALTY 4
#define HEURISTIC_TICKS_PER_PX 0.5f // < 60 / walk speed: never overestimates
#define STUCK_TICKS 120        // no change of node for this long: the edge being played is given up
#define MACRO_MAX_TICKS 240
#define BAN_TICKS (20 * TICK_HZ)
#define NO_PATH_TICKS 45       // an unreachable goal is not searched again for this long
#define SETTLE_TOL 2.0f
#define SETTLE_SPEED 12.0f

// ---- Nodes -------------------------------------------------------------------------------------

static i32 node_key(const Nav *nav, const NavNode *n) {
    return ((i32)n->ty * nav->width + n->tx) * 2 + (i32)n->kind;
}

i32 nav_find(const Nav *nav, i32 tx, i32 ty, NavNodeKind kind) {
    if (tx < 0 || ty < 0 || tx >= nav->width) {
        return -1;
    }
    const i32 key = (ty * nav->width + tx) * 2 + (i32)kind;
    i32 lo = 0, hi = (i32)nav->node_count - 1;
    while (lo <= hi) {
        const i32 mid = (lo + hi) / 2;
        const i32 k = node_key(nav, &nav->nodes[mid]);
        if (k == key) {
            return mid;
        }
        if (k < key) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}

Vec2 nav_node_feet(const Nav *nav, i32 node) {
    const NavNode *n = &nav->nodes[node];
    return vec2((f32)(n->tx * TILE_SIZE + TILE_SIZE / 2), (f32)((n->ty + 1) * TILE_SIZE));
}

static bool solid_terrain(const Level *lv, i32 tx, i32 ty) {
    return level_tile(lv, tx, ty) == TILE_SOLID;
}

// A body fits (two cells high, nothing solid or hazardous) with its feet in the cell, and something holds it.
static bool standable(const Level *lv, i32 tx, i32 ty) {
    if (solid_terrain(lv, tx, ty) || solid_terrain(lv, tx, ty - 1)) {
        return false;
    }
    if (level_tile(lv, tx, ty) == TILE_HAZARD || level_tile(lv, tx, ty - 1) == TILE_HAZARD) {
        return false;
    }
    return solid_terrain(lv, tx, ty + 1) || level_is_one_way(lv, tx, ty + 1);
}

// Which node a simulated body is on: exact cell, no fallback.
static i32 classify_exact(const Nav *nav, const Level *lv, const Companion *c) {
    const i32 tx = level_px_to_tile(c->body.pos.x + c->body.size.x * 0.5f);
    const i32 row = level_px_to_tile(c->body.pos.y + c->body.size.y - 0.001f);
    if (c->on_ladder) {
        // Standing on the top of a ladder and grabbing it: the feet are still in the cell above.
        return nav_find(nav, tx, level_tile(lv, tx, row) == TILE_LADDER ? row : row + 1, NAV_LADDER);
    }
    return c->body.on_ground ? nav_find(nav, tx, row, NAV_GROUND) : -1;
}

i32 nav_node_of(const Nav *nav, const Level *lv, const Companion *c) {
    const i32 exact = classify_exact(nav, lv, c);
    if (exact >= 0 || c->on_ladder || !c->body.on_ground) {
        return exact;
    }
    // Resting on something that is not terrain (crates), or hanging a little over a ledge: the closest
    // standing place under the body, in its column or the next ones.
    const i32 tx = level_px_to_tile(c->body.pos.x + c->body.size.x * 0.5f);
    const i32 row = level_px_to_tile(c->body.pos.y + c->body.size.y - 0.001f);
    for (i32 down = 0; down <= 4; down++) {
        static const i32 SIDE[3] = {0, -1, 1};
        for (i32 k = 0; k < 3; k++) {
            const i32 n = nav_find(nav, tx + SIDE[k], row + down, NAV_GROUND);
            if (n >= 0) {
                return n;
            }
        }
    }
    return -1;
}

i32 nav_nearest(const Nav *nav, Vec2 feet) {
    i32 best = -1;
    f32 best_score = 0.0f;
    for (i32 i = 0; i < (i32)nav->node_count; i++) {
        if (nav->nodes[i].kind != NAV_GROUND) {
            continue;
        }
        const Vec2 p = nav_node_feet(nav, i);
        const f32 score = fabsf(p.x - feet.x) + 3.0f * fabsf(p.y - feet.y);
        if (best < 0 || score < best_score) {
            best = i;
            best_score = score;
        }
    }
    return best;
}

// ---- Edges -------------------------------------------------------------------------------------

u32 nav_edge_mask(const NavEdge *e, u32 t) {
    u32 mask = 0;
    if (e->dir < 0) {
        mask |= action_bit(ACT_LEFT);
    } else if (e->dir > 0) {
        mask |= action_bit(ACT_RIGHT);
    }
    if (e->kind == NAV_DROP) {
        if (t < e->hold) {
            mask |= action_bit(ACT_DOWN) | action_bit(ACT_JUMP);
        }
        return mask;
    }
    if (e->vert < 0) {
        mask |= action_bit(ACT_UP);
    } else if (e->vert > 0) {
        mask |= action_bit(ACT_DOWN);
    }
    if (t < e->hold) {
        mask |= action_bit(ACT_JUMP);
    }
    return mask;
}

// Plays the canned input of `m` from the node `start`, `dx` pixels off its center, with the real physics.
// Returns the node where the soldier first comes to rest (or hangs on a ladder) other than `start`, or -1.
static i32 simulate(const Nav *nav, const Level *view, i32 start, const NavEdge *m, f32 dx, bool carrying,
                    u32 *ticks) {
    const NavNode *n = &nav->nodes[start];
    Companion c;
    const Vec2 feet = nav_node_feet(nav, start);
    companion_init(&c, vec2(feet.x + dx, feet.y));
    c.body.on_ground = n->kind == NAV_GROUND;
    c.on_ladder = n->kind == NAV_LADDER;
    if (carrying) {
        c.carrying = (EntityId){1, 1}; // only "is something carried" matters to the physics
    }
    InputFrame in = {0};
    // Let it settle: the start must be a real resting state.
    for (i32 t = 0; t < 2; t++) {
        input_frame_update(&in, 0);
        companion_update(&c, view, &in);
    }
    if (classify_exact(nav, view, &c) != start || fabsf(c.body.pos.x + c.body.size.x * 0.5f - (feet.x + dx)) > 0.5f) {
        return SIM_BAD_START; // it did not stay put (pushed out of a wall, fell...)
    }
    bool left = false;
    for (u32 t = 0; t < SIM_MAX_TICKS; t++) {
        input_frame_update(&in, nav_edge_mask(m, t));
        companion_update(&c, view, &in);
        if (companion_in_hazard(&c, view)) {
            return -1;
        }
        const i32 at = classify_exact(nav, view, &c);
        if (at >= 0 && at != start) {
            *ticks = t + 1;
            return at;
        }
        if (at < 0) {
            left = true;
        } else if (left) {
            return -1; // came back to where it started
        }
        if (!left && t >= SIM_STILL_TICKS) {
            return -1;
        }
    }
    return -1;
}

typedef struct Candidate {
    NavEdge e; // e.who: the one case (free / carrying) this candidate was found for
} Candidate;

static void add_candidate(Candidate *cands, u32 *count, const Nav *nav, const Level *view, i32 from, NavEdge m) {
    for (u32 who = NAV_FREE; who <= NAV_CARRY; who <<= 1) {
        if (*count >= MAX_CANDIDATES) {
            return;
        }
        u32 ticks = 0;
        const i32 to = simulate(nav, view, from, &m, 0.0f, who == NAV_CARRY, &ticks);
        if (to < 0) { // nowhere, or (SIM_BAD_START) not even a resting start
            continue;
        }
        NavEdge e = m;
        e.from = (u16)from;
        e.to = (u16)to;
        e.who = (u8)who;
        e.cost = (u16)(ticks + (m.kind == NAV_JUMP ? JUMP_PENALTY : m.kind == NAV_DROP ? DROP_PENALTY : 0));
        cands[(*count)++].e = e;
    }
}

// The move must lead to the same place from a little to either side of the node's center (a start that
// does not fit, against a wall, is not held against it).
static bool robust(const Nav *nav, const Level *view, const NavEdge *e, bool carrying) {
    u32 ticks;
    for (i32 side = -1; side <= 1; side += 2) {
        const i32 to = simulate(nav, view, e->from, e, (f32)side * ROBUST_OFFSET, carrying, &ticks);
        if (to != e->to && to != SIM_BAD_START) {
            return false;
        }
    }
    return true;
}

static bool same_move(const NavEdge *a, const NavEdge *b) {
    return a->to == b->to && a->kind == b->kind && a->dir == b->dir && a->vert == b->vert && a->hold == b->hold;
}

// All the moves from one node: every canned input is simulated, for a free soldier and for one carrying a crate;
// the cheapest robust move to each target is kept, for each of the two cases.
static bool gen_edges(Nav *nav, const Level *view, i32 idx) {
    const NavNode *n = &nav->nodes[idx];
    Candidate cands[MAX_CANDIDATES];
    u32 nc = 0;

    static const i8 DIRS[3] = {-1, 1, 0};
    static const u8 HOLDS[] = {5, 9, 13, 18, 24};
    if (n->kind == NAV_GROUND) {
        add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_WALK, .dir = -1});
        add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_WALK, .dir = 1});
        if (level_is_one_way(view, n->tx, n->ty + 1)) {
            for (u32 d = 0; d < 3; d++) {
                add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_DROP, .dir = DIRS[d], .hold = 1});
            }
        }
        if (level_tile(view, n->tx, n->ty) == TILE_LADDER) {
            add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_CLIMB, .vert = -1});
        }
        if (level_tile(view, n->tx, n->ty + 1) == TILE_LADDER) {
            add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_CLIMB, .vert = 1});
        }
    } else {
        add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_CLIMB, .vert = -1});
        add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_CLIMB, .vert = 1});
    }
    for (u32 d = 0; d < 3; d++) {
        if (n->kind == NAV_LADDER && DIRS[d] == 0) {
            continue; // a jump straight up from a ladder is just climbing
        }
        for (u32 h = 0; h < sizeof(HOLDS); h++) {
            add_candidate(cands, &nc, nav, view, idx, (NavEdge){.kind = NAV_JUMP, .dir = DIRS[d], .hold = HOLDS[h]});
        }
    }

    // Cheapest first.
    for (u32 i = 1; i < nc; i++) {
        Candidate key = cands[i];
        u32 j = i;
        while (j > 0 && cands[j - 1].e.cost > key.e.cost) {
            cands[j] = cands[j - 1];
            j--;
        }
        cands[j] = key;
    }

    const u16 first = nav->edge_count;
    for (u32 i = 0; i < nc; i++) {
        const NavEdge *c = &cands[i].e;
        NavEdge *same = NULL;
        bool covered = false;
        for (u16 k = first; k < nav->edge_count; k++) {
            NavEdge *e = &nav->edges[k];
            if (e->to == c->to && (e->who & c->who)) {
                covered = true; // a cheaper move already serves this target for this case
            }
            if (same_move(e, c)) {
                same = e;
            }
        }
        if (covered || (c->kind != NAV_WALK && !robust(nav, view, c, c->who == NAV_CARRY))) {
            continue;
        }
        if (same) {
            same->who |= c->who; // the same input works for the other case too
            continue;
        }
        if (nav->edge_count >= NAV_MAX_EDGES) {
            return false;
        }
        nav->edges[nav->edge_count++] = *c;
    }
    nav->nodes[idx].first_edge = first;
    nav->nodes[idx].edge_count = (u8)MIN((u32)(nav->edge_count - first), 255u);
    return true;
}

void nav_build(Nav *nav, const Level *lv) {
    memset(nav, 0, sizeof(*nav));
    nav->built = true;
    nav->width = lv->width;

    // The simulations must not see crates.
    Level view = *lv;
    view.dyn_solid = NULL;

    for (i32 ty = 0; ty < lv->height; ty++) {
        for (i32 tx = 0; tx < lv->width; tx++) {
            for (i32 kind = 0; kind < 2; kind++) {
                const bool is_node = kind == NAV_GROUND ? standable(&view, tx, ty)
                                                         : level_tile(&view, tx, ty) == TILE_LADDER;
                if (!is_node) {
                    continue;
                }
                if (nav->node_count >= NAV_MAX_NODES) {
                    LOG_WARN("level '%s' has too many standing places for the navigation graph (max %d)", lv->name,
                             NAV_MAX_NODES);
                    nav->node_count = 0;
                    nav->edge_count = 0;
                    return;
                }
                nav->nodes[nav->node_count++] = (NavNode){(i16)tx, (i16)ty, (u8)kind, 0, 0};
            }
        }
    }
    for (i32 i = 0; i < (i32)nav->node_count; i++) {
        if (!gen_edges(nav, &view, i)) {
            LOG_WARN("level '%s' has too many moves for the navigation graph (max %d)", lv->name, NAV_MAX_EDGES);
            nav->node_count = 0;
            nav->edge_count = 0;
            return;
        }
    }
    nav->ok = nav->node_count > 0;
}

// ---- A* ----------------------------------------------------------------------------------------

enum { UNSEEN = 0, OPEN = 1, CLOSED = 2 };

static bool heap_less(const Nav *nav, u16 a, u16 b) {
    return nav->f[a] < nav->f[b] || (nav->f[a] == nav->f[b] && a < b);
}

static void heap_swap(Nav *nav, i32 i, i32 j) {
    const u16 a = nav->heap[i], b = nav->heap[j];
    nav->heap[i] = b;
    nav->heap[j] = a;
    nav->heap_pos[b] = (i16)i;
    nav->heap_pos[a] = (i16)j;
}

static void heap_up(Nav *nav, i32 i) {
    while (i > 0) {
        const i32 parent = (i - 1) / 2;
        if (!heap_less(nav, nav->heap[i], nav->heap[parent])) {
            break;
        }
        heap_swap(nav, i, parent);
        i = parent;
    }
}

static void heap_down(Nav *nav, i32 i) {
    for (;;) {
        const i32 l = 2 * i + 1, r = l + 1;
        i32 m = i;
        if (l < nav->heap_n && heap_less(nav, nav->heap[l], nav->heap[m])) {
            m = l;
        }
        if (r < nav->heap_n && heap_less(nav, nav->heap[r], nav->heap[m])) {
            m = r;
        }
        if (m == i) {
            return;
        }
        heap_swap(nav, i, m);
        i = m;
    }
}

static void heap_push(Nav *nav, u16 node) {
    nav->heap[nav->heap_n] = node;
    nav->heap_pos[node] = (i16)nav->heap_n;
    nav->heap_n++;
    heap_up(nav, (i32)nav->heap_n - 1);
}

static u16 heap_pop(Nav *nav) {
    const u16 top = nav->heap[0];
    nav->heap_n--;
    if (nav->heap_n > 0) {
        nav->heap[0] = nav->heap[nav->heap_n];
        nav->heap_pos[nav->heap[0]] = 0;
        heap_down(nav, 0);
    }
    return top;
}

i32 nav_path(Nav *nav, i32 from, i32 goal, bool carrying, i32 banned_edge, u16 *path, u32 cap) {
    if (!nav->ok || from < 0 || goal < 0 || from >= (i32)nav->node_count || goal >= (i32)nav->node_count) {
        return -1;
    }
    if (from == goal) {
        return 0;
    }
    const i32 gx = nav->nodes[goal].tx;
    memset(nav->state, UNSEEN, nav->node_count);
    nav->heap_n = 0;
    nav->g[from] = 0.0f;
    nav->f[from] = HEURISTIC_TICKS_PER_PX * (f32)(abs(nav->nodes[from].tx - gx) * TILE_SIZE);
    nav->state[from] = OPEN;
    heap_push(nav, (u16)from);

    bool found = false;
    while (nav->heap_n > 0) {
        const u16 u = heap_pop(nav);
        nav->state[u] = CLOSED;
        if (u == goal) {
            found = true;
            break;
        }
        const NavNode *nd = &nav->nodes[u];
        for (u16 k = 0; k < nd->edge_count; k++) {
            const u16 ei = (u16)(nd->first_edge + k);
            const NavEdge *e = &nav->edges[ei];
            if (!(e->who & (carrying ? NAV_CARRY : NAV_FREE)) || (i32)ei == banned_edge || nav->state[e->to] == CLOSED) {
                continue;
            }
            const f32 ng = nav->g[u] + (f32)e->cost;
            if (nav->state[e->to] == UNSEEN || ng < nav->g[e->to]) {
                nav->g[e->to] = ng;
                nav->f[e->to] = ng + HEURISTIC_TICKS_PER_PX * (f32)(abs(nav->nodes[e->to].tx - gx) * TILE_SIZE);
                nav->parent_edge[e->to] = ei;
                if (nav->state[e->to] == UNSEEN) {
                    nav->state[e->to] = OPEN;
                    heap_push(nav, e->to);
                } else {
                    heap_up(nav, nav->heap_pos[e->to]);
                }
            }
        }
    }
    if (!found) {
        return -1;
    }
    // Walk back from the goal (the heap array is free again), then copy forward.
    u32 count = 0;
    for (u16 v = (u16)goal; v != from; v = nav->edges[nav->parent_edge[v]].from) {
        nav->heap[count++] = nav->parent_edge[v];
    }
    const u32 n = MIN(count, cap);
    for (u32 i = 0; i < n; i++) {
        path[i] = nav->heap[count - 1 - i];
    }
    return (i32)n;
}

// ---- Path follower -----------------------------------------------------------------------------

u32 nav_steer_x(const Companion *c, f32 x) {
    const f32 cx = c->body.pos.x + c->body.size.x * 0.5f;
    const f32 vx = c->body.vel.x;
    // Where it would stop if the keys were released now.
    const f32 stop = cx + (vx >= 0.0f ? 1.0f : -1.0f) * vx * vx / (2.0f * TUNING_GROUND_DECEL);
    const f32 want = x - stop;
    if (fabsf(want) <= 1.0f) {
        return 0;
    }
    return action_bit(want > 0.0f ? ACT_RIGHT : ACT_LEFT);
}

bool nav_settled_at(const Companion *c, f32 x, f32 tol) {
    return fabsf(x - (c->body.pos.x + c->body.size.x * 0.5f)) <= tol && fabsf(c->body.vel.x) <= SETTLE_SPEED;
}

static void drop_path(NavAgent *a) {
    a->path_len = 0;
    a->path_pos = 0;
    a->in_macro = false;
    a->airborne = false;
    a->macro_tick = 0;
    a->fails = 0;
}

// The soldier is at node `cur` but not where the path expects: is `cur` further along the path?
static bool realign(const Nav *nav, NavAgent *a, i32 cur) {
    for (u32 k = a->path_pos; k < a->path_len; k++) {
        if (nav->edges[a->path[k]].from == cur) {
            a->path_pos = (u8)k;
            return true;
        }
    }
    return false;
}

static void ban_edge(NavAgent *a, u16 edge) {
    a->banned1 = (u16)(edge + 1);
    a->banned_ticks = BAN_TICKS;
}

NavStatus nav_steer(Nav *nav, NavAgent *a, const Level *lv, const Companion *c, Vec2 goal_feet, u32 *mask) {
    *mask = 0;
    if (!nav->ok) {
        return NAV_NO_PATH;
    }
    if (a->banned_ticks > 0 && --a->banned_ticks == 0) {
        a->banned1 = 0;
    }
    if (a->fail_ticks > 0) {
        a->fail_ticks--;
    }
    const i32 goal = nav_nearest(nav, goal_feet);
    if (goal < 0) {
        return NAV_NO_PATH;
    }
    if (a->goal1 != goal + 1) {
        drop_path(a);
        a->goal1 = (u16)(goal + 1);
        a->fail_ticks = 0;
    }
    const i32 cur = nav_node_of(nav, lv, c);
    if (cur == goal) {
        drop_path(a);
        a->last_node1 = (u16)(cur + 1);
        return NAV_ARRIVED;
    }
    if (a->fail_ticks > 0) {
        return NAV_NO_PATH;
    }

    a->progress++;
    if (cur >= 0 && cur + 1 != a->last_node1) {
        a->last_node1 = (u16)(cur + 1);
        a->progress = 0;
    }
    if (a->progress > STUCK_TICKS || a->macro_tick > MACRO_MAX_TICKS) {
        if (a->path_pos < a->path_len) {
            ban_edge(a, a->path[a->path_pos]);
        }
        drop_path(a);
        a->progress = 0;
    }

    if (cur >= 0) {
        if (a->in_macro) {
            const NavEdge *e = &nav->edges[a->path[a->path_pos]];
            if (cur != e->from || a->airborne) { // the canned input is over: where did it lead?
                const bool airborne = a->airborne;
                a->in_macro = false;
                a->macro_tick = 0;
                a->airborne = false;
                if (cur == e->to) {
                    a->path_pos++;
                    a->fails = 0;
                } else if (!realign(nav, a, cur)) {
                    if (airborne && cur == e->from && ++a->fails < 2) {
                        // Landed where it started: try the same move once more.
                    } else {
                        if (cur == e->from) {
                            ban_edge(a, a->path[a->path_pos]);
                        }
                        drop_path(a);
                    }
                }
            }
        } else if (a->path_pos < a->path_len && nav->edges[a->path[a->path_pos]].from != cur && !realign(nav, a, cur)) {
            drop_path(a);
        }
        if (a->path_pos >= a->path_len) {
            drop_path(a);
        }
    } else if (!a->in_macro) {
        return NAV_MOVING; // falling or thrown: nothing to do but wait
    } else {
        a->airborne = true;
    }

    if (a->path_len == 0) {
        const i32 n = nav_path(nav, cur, goal, entity_id_valid(c->carrying), (i32)a->banned1 - 1, a->path, NAV_PATH_MAX);
        if (n < 0) {
            a->fail_ticks = NO_PATH_TICKS;
            return NAV_NO_PATH;
        }
        a->path_len = (u8)n;
        a->path_pos = 0;
        a->progress = 0;
        a->fails = 0;
        if (n == 0) {
            return NAV_ARRIVED;
        }
    }

    const NavEdge *e = &nav->edges[a->path[a->path_pos]];
    if (!a->in_macro) {
        const bool needs_footing = e->kind != NAV_WALK && nav->nodes[e->from].kind == NAV_GROUND;
        if (needs_footing) {
            const f32 x = nav_node_feet(nav, e->from).x;
            if (!nav_settled_at(c, x, SETTLE_TOL)) {
                *mask = nav_steer_x(c, x);
                return NAV_MOVING;
            }
        }
        a->in_macro = true;
        a->airborne = false;
        a->macro_tick = 0;
    }
    if (e->kind == NAV_WALK) {
        // Aim past the target cell so that it never stops there; the walker hops what is in the way.
        const f32 to_x = nav_node_feet(nav, e->to).x;
        a->walker.target_x = to_x + (f32)e->dir * (f32)TILE_SIZE;
        InputFrame tmp = {0};
        walker_think(&a->walker, lv, c, &tmp);
        *mask = tmp.down;
    } else {
        *mask = nav_edge_mask(e, a->macro_tick);
    }
    a->macro_tick++;
    return NAV_MOVING;
}
