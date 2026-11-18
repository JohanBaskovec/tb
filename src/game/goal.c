#include "game/goal.h"

void goal_set(GameState *gs, i32 team, PadShape shape, bool blue) {
    if (team < 0 || team >= MAX_TEAMS || shape < 0 || shape >= SHAPE_COUNT) {
        return;
    }
    gs->teams[team].goal = (BuildGoal){.active = true, .shape = (i32)shape, .blue = blue};
}

void goal_clear(GameState *gs, i32 team) {
    if (team >= 0 && team < MAX_TEAMS) {
        gs->teams[team].goal = (BuildGoal){0};
    }
}

// Level 0 is the bottom row of the pad, level 1 the one above it.
static i32 level_of_row(i32 row) {
    return PAD_ROWS - 1 - row;
}

static bool cell_filled(const PadGrid *grid, i32 col, i32 level) {
    return grid->cells[PAD_ROWS - 1 - level][col] != PAD_EMPTY;
}

typedef struct Candidate {
    u32 missing;
    u32 blue;
    u8 wanted[PAD_COLS];
} Candidate;

// Can crates be put in the columns of `wanted` coming from `entry`? Every column before them must hold no crate.
static bool enterable(const u8 filled[PAD_COLS], const u8 wanted[PAD_COLS], i32 entry) {
    if (entry == 0) {
        return true;
    }
    for (i32 col = 0; col < PAD_COLS; col++) {
        if (!wanted[col]) {
            continue;
        }
        for (i32 k = entry > 0 ? 0 : col + 1; k < (entry > 0 ? col : PAD_COLS); k++) {
            if (filled[k] != 0) {
                return false;
            }
        }
    }
    return true;
}

// What it takes to complete `p` from the crates of `grid`. False if it cannot be completed: a missing cell would have
// nothing under it, or the goal is blue and there is no free cell left to put the blue crate in.
static bool evaluate(const PadGrid *grid, const PadPlacement *p, bool needs_blue, Candidate *out) {
    *out = (Candidate){0};
    for (u32 i = 0; i < p->n; i++) {
        const i32 col = p->c[i].col, level = level_of_row(p->c[i].row);
        const PadCell cell = (PadCell)grid->cells[p->c[i].row][col];
        if (cell == PAD_EMPTY) {
            out->missing++;
            out->wanted[col] |= (u8)(1u << level);
        } else if (cell == PAD_BLUE) {
            out->blue++;
        }
    }
    for (i32 col = 0; col < PAD_COLS; col++) { // a crate cannot be put where nothing holds it
        for (i32 level = 1; level < PAD_ROWS; level++) {
            if ((out->wanted[col] & (1u << level)) && !cell_filled(grid, col, level - 1) &&
                !(out->wanted[col] & (1u << (level - 1)))) {
                return false;
            }
        }
    }
    return !(needs_blue && out->blue == 0 && out->missing == 0);
}

// The best position of the shape from one side: the fewest crates to bring; among those, the one that already has a blue
// crate (a blue goal). False if none can be completed from there.
static bool best_position(const GameState *gs, const PadGrid *grid, const u8 filled[PAD_COLS], const BuildGoal *goal, i32 entry,
                          Candidate *best) {
    PadPlacement placements[PAD_MAX_PLACEMENTS];
    const u32 count = pad_shape_placements((PadShape)goal->shape, placements);
    (void)gs;
    bool found = false;
    for (u32 i = 0; i < count; i++) {
        Candidate c;
        if (!evaluate(grid, &placements[i], goal->blue, &c) || !enterable(filled, c.wanted, entry)) {
            continue;
        }
        if (!found || c.missing < best->missing || (c.missing == best->missing && goal->blue && c.blue > best->blue)) {
            *best = c;
            found = true;
        }
    }
    return found;
}

void goal_plan(const GameState *gs, i32 team, i32 entry, GoalPlan *out) {
    *out = (GoalPlan){0};
    const BuildGoal *goal = &gs->teams[team].goal;
    PadGrid grid;
    if (!goal->active || !pad_usable(gs, team) || !pad_read_grid(gs, team, &grid, NULL)) {
        return;
    }
    out->valid = true;
    for (i32 col = 0; col < PAD_COLS; col++) {
        for (i32 level = 0; level < PAD_ROWS; level++) {
            out->filled[col] = (u8)(out->filled[col] + (cell_filled(&grid, col, level) ? 1 : 0));
        }
    }
    PadOptionList options;
    pad_options(&grid, &options);
    const PadOption *red = NULL, *blue = NULL;
    for (u32 i = 0; i < options.count; i++) {
        if (options.items[i].shape != (PadShape)goal->shape) {
            continue;
        }
        if (options.items[i].blue) {
            blue = &options.items[i];
        } else {
            red = &options.items[i];
        }
    }
    if (goal->blue ? blue != NULL : (red != NULL || blue != NULL)) {
        out->buildable = true;
        out->build_blue = goal->blue || red == NULL;
        out->feasible = true;
        return;
    }
    Candidate best = {0};
    bool found = best_position(gs, &grid, out->filled, goal, entry, &best);
    out->side = entry;
    if (!found && entry != 0) { // from the other side, if the pad can be walked around
        u8 tallest = 0;
        for (i32 col = 0; col < PAD_COLS; col++) {
            tallest = MAX(tallest, out->filled[col]);
        }
        if (tallest <= 1 && best_position(gs, &grid, out->filled, goal, -entry, &best)) {
            found = true;
            out->side = -entry;
        }
    }
    out->feasible = found;
    if (found) {
        out->missing = best.missing;
        out->needs_blue = goal->blue && best.blue == 0;
        for (i32 col = 0; col < PAD_COLS; col++) {
            out->wanted[col] = best.wanted[col];
        }
    }
}

static bool has_ally(const GameState *gs, i32 team) {
    for (i32 i = 0; i < MAX_SOLDIERS; i++) {
        if (gs->soldiers[i].alive && gs->soldiers[i].team == team && i != gs->controlled) {
            return true;
        }
    }
    return false;
}

void goal_update(GameState *gs) {
    for (i32 team = 0; team < MAX_TEAMS; team++) {
        const Team *t = &gs->teams[team];
        if (!t->goal.active || t->order != ORDER_COLLECT || !has_ally(gs, team)) {
            continue;
        }
        GoalPlan plan;
        goal_plan(gs, team, 0, &plan);
        if (plan.buildable && pad_build(gs, team, (PadShape)t->goal.shape, plan.build_blue)) {
            goal_clear(gs, team); // done
        }
    }
}
