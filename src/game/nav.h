// Navigation: a graph of the places a soldier can stand, A* over it, and a path follower. No SDL.
//
// The graph is built by SIMULATION: from every standing place the real physics (companion_update) is run
// with a few canned inputs (walk, jumps of several heights, drop through a platform, climb), and the place
// where the soldier ends up becomes the target of an edge. So the graph can never promise a move the physics
// does not allow, and changing the tuning (jump height, speed...) changes the graph with it. Edges are
// built without any crate on the ground; the follower deals with crates as obstacles it can hop over.
//
// The follower turns a path into an ordinary InputFrame mask: the AI can do nothing a human cannot.
#pragma once

#include "core/input_frame.h"
#include "game/companion.h"
#include "game/walker.h"

#define NAV_MAX_NODES 1024
#define NAV_MAX_EDGES 10240
#define NAV_PATH_MAX 40 // edges kept in a path; a longer path is followed in several pieces

typedef enum NavNodeKind {
    NAV_GROUND, // standing on the floor, the feet in the cell (tx, ty)
    NAV_LADDER, // hanging on the ladder cell (tx, ty)
} NavNodeKind;

typedef struct NavNode {
    i16 tx, ty;
    u8 kind;
    u8 edge_count;
    u16 first_edge;
} NavNode;

#define NAV_FREE 1u
#define NAV_CARRY 2u

typedef enum NavEdgeKind {
    NAV_WALK,  // hold a direction: one cell further, or off a ledge and down
    NAV_JUMP,  // hold a direction and jump for `hold` ticks
    NAV_DROP,  // down + jump on a thin platform, then fall
    NAV_CLIMB, // up / down on a ladder (and grabbing or leaving it)
} NavEdgeKind;

// One canned input sequence and where it leads. The same fields drive the simulation that finds the edge
// and the follower that executes it (nav_edge_mask), so they cannot disagree.
typedef struct NavEdge {
    u16 from, to;
    u8 kind;
    i8 dir;       // -1 / 0 / +1: horizontal key held during the whole move
    i8 vert;      // -1: up held, +1: down held (climbing)
    u8 hold;      // ticks the jump key is held at the start (for a drop: jump and down together)
    u16 cost;     // ticks the move took in the simulation (plus a small penalty for risky moves)
    u8 who;       // NAV_FREE / NAV_CARRY bits: who the move works for (a crate carrier walks slower, jumps lower)
} NavEdge;

typedef struct Nav {
    bool built; // a build was attempted (lazy: see nav_ensure)
    bool ok;    // the graph can be used
    i32 width;  // level width in tiles
    u16 node_count, edge_count;
    NavNode nodes[NAV_MAX_NODES];
    NavEdge edges[NAV_MAX_EDGES];
    // A* scratch space (not part of the game state: rebuilt by every query).
    f32 g[NAV_MAX_NODES];
    f32 f[NAV_MAX_NODES];
    u16 parent_edge[NAV_MAX_NODES];
    u8 state[NAV_MAX_NODES];
    u16 heap[NAV_MAX_NODES];
    i16 heap_pos[NAV_MAX_NODES];
    u16 heap_n;
} Nav;

// Builds the graph of the level (terrain only: crates are ignored). `ok` stays false when the level has no
// standing place or too many of them (the AI then falls back to walking straight at its target).
void nav_build(Nav *nav, const Level *lv);

// Node index of a place, or -1.
i32 nav_find(const Nav *nav, i32 tx, i32 ty, NavNodeKind kind);
// The node the soldier is on right now, or -1 while it is in the air. Standing on a stack of crates counts as
// standing on the floor under it.
i32 nav_node_of(const Nav *nav, const Level *lv, const Companion *c);
// The standing node closest to a point (x, y = where the feet would be); -1 if there is none.
i32 nav_nearest(const Nav *nav, Vec2 feet);
// Bottom-center of the cell of a node, in pixels.
Vec2 nav_node_feet(const Nav *nav, i32 node);

// Keys of an edge at tick `t` of its execution.
u32 nav_edge_mask(const NavEdge *e, u32 t);

// A*: fills `path` with up to `cap` edge indices from `from` to `goal` and returns their number (0 when already
// there), or -1 when there is no path. `banned_edge` (or -1) is left out. A carrying soldier only uses
// NAV_CARRY edges, the others NAV_FREE ones.
i32 nav_path(Nav *nav, i32 from, i32 goal, bool carrying, i32 banned_edge, u16 *path, u32 cap);

// ---- Path follower -----------------------------------------------------------------------------

// What one soldier remembers of the trip it is on. All-zero = no trip.
typedef struct NavAgent {
    Walker walker;       // walking edges (hops small obstacles such as crates)
    u16 goal1;           // goal node + 1 (0 = none)
    u16 path[NAV_PATH_MAX];
    u8 path_len, path_pos;
    bool in_macro;       // the canned input of path[path_pos] is being played
    bool airborne;       // ... and the soldier has left the node
    u8 fails;            // times the current edge ended where it started
    u32 macro_tick;
    u32 progress;        // ticks since the soldier last changed node
    u16 last_node1;      // node + 1 of the last tick
    u16 banned1;         // edge + 1 that did not work: avoided for a while
    u32 banned_ticks;
    u32 fail_ticks;      // > 0: the current goal has no path, do not search again yet
} NavAgent;

typedef enum NavStatus {
    NAV_MOVING,  // `mask` holds the keys of this tick
    NAV_ARRIVED, // standing on the goal node
    NAV_NO_PATH, // the goal cannot be reached (or there is no usable graph)
} NavStatus;

// One tick of travelling to the node nearest to `goal_feet`. Replans when the goal changes, when the soldier
// ends up off its path, or when an edge does not work.
NavStatus nav_steer(Nav *nav, NavAgent *a, const Level *lv, const Companion *c, Vec2 goal_feet, u32 *mask);
// Keys that bring the soldier to rest at `x` (center of the body): it brakes in time instead of overshooting.
u32 nav_steer_x(const Companion *c, f32 x);
// Standing still within `tol` pixels of `x`.
bool nav_settled_at(const Companion *c, f32 x, f32 tol);
