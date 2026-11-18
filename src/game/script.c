#include "game/script.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool key_bit(char c, u32 *bit) {
    switch (c) {
    case 'L':
        *bit = action_bit(ACT_LEFT);
        return true;
    case 'R':
        *bit = action_bit(ACT_RIGHT);
        return true;
    case 'U':
        *bit = action_bit(ACT_UP);
        return true;
    case 'D':
        *bit = action_bit(ACT_DOWN);
        return true;
    case 'J':
        *bit = action_bit(ACT_JUMP);
        return true;
    case 'F':
        *bit = action_bit(ACT_FIRE);
        return true;
    case 'G':
        *bit = action_bit(ACT_GRAB);
        return true;
    case 'S':
        *bit = action_bit(ACT_SWITCH);
        return true;
    case 'O':
        *bit = action_bit(ACT_ORDER);
        return true;
    case 'P':
        *bit = action_bit(ACT_PAUSE);
        return true;
    case 'C':
        *bit = action_bit(ACT_CONFIRM);
        return true;
    case 'B':
        *bit = action_bit(ACT_DESTROY);
        return true;
    default:
        return false;
    }
}

// Copy of a token for an error message (static buffer: errors are reported one at a time).
static const char *tok_copy(const char *tok, usize len) {
    static char copy[64];
    usize n = len < sizeof(copy) - 1 ? len : sizeof(copy) - 1;
    memcpy(copy, tok, n);
    copy[n] = '\0';
    return copy;
}

static void fail(char *err, usize err_len, const char *fmt, const char *token) {
    if (err && err_len > 0) {
        snprintf(err, err_len, fmt, token);
    }
}

bool script_parse(Script *s, const char *text, char *err, usize err_len) {
    memset(s, 0, sizeof(*s));
    const char *p = text;
    while (*p != '\0') {
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        const char *tok = p;
        while (*p != '\0' && *p != ' ' && *p != ',' && *p != '\t' && *p != '\n' && *p != '\r') {
            p++;
        }
        char buf[64];
        usize tlen = (usize)(p - tok);
        if (tlen >= sizeof(buf)) {
            fail(err, err_len, "step too long: '%s'", "...");
            return false;
        }
        memcpy(buf, tok, tlen);
        buf[tlen] = '\0';

        char *colon = strchr(buf, ':');
        if (!colon || colon == buf || colon[1] == '\0') {
            fail(err, err_len, "bad step '%s' (expected KEYS:TICKS)", buf);
            return false;
        }
        *colon = '\0';
        bool has_aim = false;
        Vec2 aim = vec2(0.0f, 0.0f);
        char *at = strchr(buf, '@');
        if (at) {
            float ax, ay;
            char extra;
            if (sscanf(at + 1, "%f/%f%c", &ax, &ay, &extra) != 2) {
                fail(err, err_len, "bad aim in '%s' (expected KEYS@X/Y:TICKS)", tok_copy(tok, tlen));
                return false;
            }
            has_aim = true;
            aim = vec2(ax, ay);
            *at = '\0';
        }
        if (buf[0] == '\0') {
            fail(err, err_len, "bad step '%s' (no keys before '@')", tok_copy(tok, tlen));
            return false;
        }
        char *end = NULL;
        unsigned long ticks = strtoul(colon + 1, &end, 10);
        if (*end != '\0' || ticks == 0 || ticks > 1000000ul) {
            fail(err, err_len, "bad tick count in '%s'", colon + 1);
            return false;
        }
        u32 mask = 0;
        if (!(buf[0] == '.' && buf[1] == '\0')) {
            for (const char *k = buf; *k != '\0'; k++) {
                u32 bit;
                if (!key_bit((char)toupper((unsigned char)*k), &bit)) {
                    char one[2] = {*k, '\0'};
                    fail(err, err_len, "unknown key '%s'", one);
                    return false;
                }
                mask |= bit;
            }
        }
        if (s->count >= SCRIPT_MAX_STEPS) {
            fail(err, err_len, "too many steps (%s)", "max 256");
            return false;
        }
        s->has_aim[s->count] = has_aim;
        s->aim[s->count] = aim;
        s->masks[s->count] = mask;
        s->ticks[s->count] = (u32)ticks;
        s->count++;
    }
    if (s->count > 0) {
        s->left = s->ticks[0];
    }
    return true;
}

bool script_finished(const Script *s) {
    return s->step >= s->count;
}

bool script_next(Script *s, InputFrame *frame) {
    if (script_finished(s)) {
        input_frame_update(frame, 0);
        frame->aim_active = false;
        return false;
    }
    input_frame_update(frame, s->masks[s->step]);
    frame->aim_active = s->has_aim[s->step];
    frame->aim = s->aim[s->step];
    s->left--;
    if (s->left == 0) {
        s->step++;
        if (s->step < s->count) {
            s->left = s->ticks[s->step];
        }
    }
    return true;
}

u32 script_total_ticks(const Script *s) {
    u32 total = 0;
    for (u32 i = 0; i < s->count; i++) {
        total += s->ticks[i];
    }
    return total;
}
