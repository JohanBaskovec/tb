#include "test.h"

#include "game/entity.h"

static EntityPool g_pool; // large: keep it off the stack

static void test_create_get_destroy(void) {
    pool_init(&g_pool);
    CHECK_EQ_U64(g_pool.live_count, 0);
    CHECK(pool_get(&g_pool, ENTITY_NONE) == NULL);

    EntityId a = pool_create(&g_pool, ENT_CRATE);
    EntityId b = pool_create(&g_pool, ENT_CRATE);
    CHECK(entity_id_valid(a) && entity_id_valid(b));
    CHECK(!entity_id_eq(a, b));
    CHECK_EQ_U64(a.index, 1); // deterministic: slots are handed out in order
    CHECK_EQ_U64(b.index, 2);
    CHECK_EQ_U64(g_pool.live_count, 2);

    Entity *ea = pool_get(&g_pool, a);
    CHECK(ea != NULL && ea->type == ENT_CRATE);
    ea->body.pos = vec2(5, 6);
    CHECK_NEAR(pool_get_const(&g_pool, a)->body.pos.x, 5.0, 0.0);

    pool_mark_destroy(&g_pool, a);
    CHECK(pool_get(&g_pool, a) != NULL); // still resolvable until the flush (deferred destruction)
    CHECK_EQ_U64(g_pool.live_count, 2);
    pool_flush(&g_pool);
    CHECK(pool_get(&g_pool, a) == NULL);
    CHECK(pool_get(&g_pool, b) != NULL);
    CHECK_EQ_U64(g_pool.live_count, 1);
}

static void test_generations_invalidate_old_ids(void) {
    pool_init(&g_pool);
    EntityId first = pool_create(&g_pool, ENT_CRATE);
    pool_mark_destroy(&g_pool, first);
    pool_flush(&g_pool);

    // The slot is reused, with a newer generation: the old id must not resolve to the newcomer.
    EntityId second = pool_create(&g_pool, ENT_CRATE);
    CHECK_EQ_U64(second.index, first.index);
    CHECK(second.generation != first.generation);
    CHECK(pool_get(&g_pool, first) == NULL);
    CHECK(pool_get(&g_pool, second) != NULL);

    // Marking a stale id must not destroy the new occupant.
    pool_mark_destroy(&g_pool, first);
    pool_flush(&g_pool);
    CHECK(pool_get(&g_pool, second) != NULL);

    // A recycled entity starts zeroed.
    pool_get(&g_pool, second)->body.vel = vec2(9, 9);
    pool_mark_destroy(&g_pool, second);
    pool_flush(&g_pool);
    EntityId third = pool_create(&g_pool, ENT_CRATE);
    CHECK_NEAR(pool_get(&g_pool, third)->body.vel.x, 0.0, 0.0);
    CHECK(!pool_get(&g_pool, third)->pending_destroy);
}

static void test_bad_ids_and_double_destroy(void) {
    pool_init(&g_pool);
    EntityId a = pool_create(&g_pool, ENT_CRATE);
    CHECK(pool_get(&g_pool, (EntityId){0, a.generation}) == NULL);
    CHECK(pool_get(&g_pool, (EntityId){MAX_ENTITIES + 1, 1}) == NULL);
    CHECK(pool_get(&g_pool, (EntityId){a.index, (u16)(a.generation + 1)}) == NULL);
    CHECK(pool_get(&g_pool, (EntityId){7, 1}) == NULL); // never created

    pool_mark_destroy(&g_pool, a);
    pool_mark_destroy(&g_pool, a); // twice: harmless
    pool_flush(&g_pool);
    pool_flush(&g_pool); // nothing left: harmless
    CHECK_EQ_U64(g_pool.live_count, 0);
    CHECK_EQ_U64(g_pool.free_count, MAX_ENTITIES);
}

static void test_full_pool(void) {
    pool_init(&g_pool);
    for (int i = 0; i < MAX_ENTITIES; i++) {
        CHECK(entity_id_valid(pool_create(&g_pool, ENT_CRATE)));
    }
    CHECK_EQ_U64(g_pool.live_count, MAX_ENTITIES);
    CHECK(!entity_id_valid(pool_create(&g_pool, ENT_CRATE))); // full: no crash, no id

    // Free one and the pool works again.
    EntityId one = {17, g_pool.slots[17].generation};
    pool_mark_destroy(&g_pool, one);
    pool_flush(&g_pool);
    EntityId again = pool_create(&g_pool, ENT_CRATE);
    CHECK_EQ_U64(again.index, 17);
    CHECK(!entity_id_valid(pool_create(&g_pool, ENT_CRATE)));
}

static void test_generation_wraps_without_becoming_none(void) {
    pool_init(&g_pool);
    g_pool.slots[1].generation = 0xFFFF;
    EntityId id = pool_create(&g_pool, ENT_CRATE);
    CHECK_EQ_U64(id.index, 1);
    CHECK(id.generation != 0); // 0 is reserved for "no entity"
    CHECK(pool_get(&g_pool, id) != NULL);
}

static void test_same_sequence_gives_same_ids(void) {
    EntityId run[2][6];
    for (int r = 0; r < 2; r++) {
        pool_init(&g_pool);
        EntityId ids[4];
        for (int i = 0; i < 4; i++) {
            ids[i] = pool_create(&g_pool, ENT_CRATE);
        }
        pool_mark_destroy(&g_pool, ids[1]);
        pool_mark_destroy(&g_pool, ids[2]);
        pool_flush(&g_pool);
        run[r][0] = pool_create(&g_pool, ENT_CRATE);
        run[r][1] = pool_create(&g_pool, ENT_CRATE);
        run[r][2] = pool_create(&g_pool, ENT_CRATE);
        run[r][3] = ids[0];
        run[r][4] = ids[3];
        run[r][5] = ids[1];
    }
    for (int i = 0; i < 6; i++) {
        CHECK(entity_id_eq(run[0][i], run[1][i]));
    }
}

int main(void) {
    RUN_TEST(test_create_get_destroy);
    RUN_TEST(test_generations_invalidate_old_ids);
    RUN_TEST(test_bad_ids_and_double_destroy);
    RUN_TEST(test_full_pool);
    RUN_TEST(test_generation_wraps_without_becoming_none);
    RUN_TEST(test_same_sequence_gives_same_ids);
    TEST_MAIN_END();
}
