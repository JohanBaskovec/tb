#include "test.h"

#include "game/script.h"

#include <string.h>

static void test_parse_ok(void) {
    Script s;
    char err[64] = "";
    CHECK(script_parse(&s, "R:60 rj:12, .:30\n U:90", err, sizeof(err)));
    CHECK_EQ_U64(s.count, 4);
    CHECK_EQ_U64(s.masks[0], action_bit(ACT_RIGHT));
    CHECK_EQ_U64(s.masks[1], action_bit(ACT_RIGHT) | action_bit(ACT_JUMP)); // lowercase accepted
    CHECK_EQ_U64(s.masks[2], 0);
    CHECK_EQ_U64(s.masks[3], action_bit(ACT_UP));
    CHECK_EQ_U64(script_total_ticks(&s), 60 + 12 + 30 + 90);

    CHECK(script_parse(&s, "", err, sizeof(err))); // empty script is valid and already finished
    CHECK(script_finished(&s));
    CHECK_EQ_U64(s.count, 0);
}

static void test_every_key(void) {
    Script s;
    char err[64] = "";
    CHECK(script_parse(&s, "LRUDJFGSOP:1", err, sizeof(err)));
    u32 all = 0;
    for (int a = 0; a < ACT_COUNT; a++) {
        all |= action_bit((Action)a);
    }
    CHECK_EQ_U64(s.masks[0], all);
}

static void test_playback_and_edges(void) {
    Script s;
    char err[64] = "";
    CHECK(script_parse(&s, "R:2 .:1 RJ:1", err, sizeof(err)));
    InputFrame f = {0};

    CHECK(script_next(&s, &f)); // tick 1: R pressed
    CHECK(input_down(&f, ACT_RIGHT) && input_pressed(&f, ACT_RIGHT));
    CHECK(script_next(&s, &f)); // tick 2: R held
    CHECK(input_down(&f, ACT_RIGHT) && !input_pressed(&f, ACT_RIGHT));
    CHECK(script_next(&s, &f)); // tick 3: nothing, R released
    CHECK(!input_down(&f, ACT_RIGHT) && input_released(&f, ACT_RIGHT));
    CHECK(script_next(&s, &f)); // tick 4: R and J pressed together
    CHECK(input_pressed(&f, ACT_RIGHT) && input_pressed(&f, ACT_JUMP));
    CHECK(script_finished(&s));
    CHECK(!script_next(&s, &f)); // over: everything released
    CHECK(f.down == 0);
    CHECK(input_released(&f, ACT_JUMP));
    CHECK(!script_next(&s, &f)); // stays finished
}

typedef struct BadCase {
    const char *text;
    const char *expected;
} BadCase;

static void test_parse_errors(void) {
    static const BadCase cases[] = {
        {"R", "KEYS:TICKS"},
        {":5", "KEYS:TICKS"},
        {"R:", "KEYS:TICKS"},
        {"R:0", "tick count"},
        {"R:abc", "tick count"},
        {"R:-3", "tick count"},
        {"X:5", "unknown key"},
        {"R:5 ?:5", "unknown key"},
    };
    for (usize i = 0; i < ARRAY_LEN(cases); i++) {
        Script s;
        char err[96] = "";
        bool ok = script_parse(&s, cases[i].text, err, sizeof(err));
        CHECK(!ok);
        if (strstr(err, cases[i].expected) == NULL) {
            fprintf(stderr, "case '%s': error '%s' does not contain '%s'\n", cases[i].text, err,
                    cases[i].expected);
            g_failures++;
        }
    }

    // Too many steps.
    static char big[SCRIPT_MAX_STEPS * 6 + 16];
    big[0] = '\0';
    for (int i = 0; i < SCRIPT_MAX_STEPS + 1; i++) {
        strcat(big, "R:1 ");
    }
    Script s;
    char err[96] = "";
    CHECK(!script_parse(&s, big, err, sizeof(err)));
    CHECK(strstr(err, "too many") != NULL);

    CHECK(!script_parse(&s, "R", NULL, 0)); // no error buffer: must not crash
}

int main(void) {
    RUN_TEST(test_parse_ok);
    RUN_TEST(test_every_key);
    RUN_TEST(test_playback_and_edges);
    RUN_TEST(test_parse_errors);
    TEST_MAIN_END();
}
