#include "game/bot.h"

void bot_init(Bot *bot, u64 seed) {
    rng_seed(&bot->rng, seed, 77);
    bot->mask = 0;
    bot->ticks_left = 0;
    bot->jump_ticks_left = 0;
}

// Random wanderer: holds a random direction for 10..60 ticks, and now and then holds jump for a
// few ticks (a short hold gives a short hop). Up/down are random too, so it also uses ladders
// and drops through platforms (down + jump). It also mashes grab and fire now and then, which
// picks up, places and throws crates.
void bot_think(Bot *bot, InputFrame *frame) {
    if (bot->ticks_left == 0) {
        bot->mask = 0;
        switch (rng_below(&bot->rng, 3)) {
        case 1:
            bot->mask |= action_bit(ACT_LEFT);
            break;
        case 2:
            bot->mask |= action_bit(ACT_RIGHT);
            break;
        default:
            break;
        }
        switch (rng_below(&bot->rng, 4)) {
        case 1:
            bot->mask |= action_bit(ACT_UP);
            break;
        case 2:
            bot->mask |= action_bit(ACT_DOWN);
            break;
        default:
            break;
        }
        bot->ticks_left = (u32)rng_range_i32(&bot->rng, 10, 60);
    }
    bot->ticks_left--;

    if (bot->jump_ticks_left > 0) {
        bot->jump_ticks_left--;
    } else if (rng_below(&bot->rng, 40) == 0) {
        bot->jump_ticks_left = (u32)rng_range_i32(&bot->rng, 2, 28);
    }
    u32 mask = bot->mask;
    if (bot->jump_ticks_left > 0) {
        mask |= action_bit(ACT_JUMP);
    }
    // One-tick presses: grab (pick up / put down) and fire (throw when carrying).
    if (rng_below(&bot->rng, 45) == 0) {
        mask |= action_bit(ACT_GRAB);
    }
    if (rng_below(&bot->rng, 90) == 0) {
        mask |= action_bit(ACT_FIRE);
    }
    input_frame_update(frame, mask);
}
