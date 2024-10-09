#include "chshg/c_hierarchical_spatial_hashgrid.h"
#include "chshg/test_allocator.h"

#include "cunittest/cunittest.h"

using namespace ncore;

struct object_t
{
    s32  dead;
    s32  count;
    bool remove;
};

class objects_t
{
    enum
    {
        EMAX_OBJECTS = 32
    };

public:
    objects_t() { init(); }

    void init()
    {
        m_free_obj   = EMAX_OBJECTS;
        m_obj_count  = 0;
        m_obj_count2 = 0;
        for (int i = 0; i < EMAX_OBJECTS; ++i)
        {
            m_objects[i].count = 0;
        }
    }

    void reset()
    {
        m_obj_count = 0;
        for (int i = 0; i < EMAX_OBJECTS; ++i)
        {
            m_objects[i].count = 0;
        }
    }

    s32 get()
    {
        ++m_obj_count;
        if (m_free_obj == EMAX_OBJECTS)
        {
            return m_obj_count2++;
        }

        s32 index              = m_free_obj;
        m_free_obj             = m_objects[index].count;
        m_objects[index].dead  = 0;
        m_objects[index].count = 0;
        return index;
    }

    void release(s32 index)
    {
        m_objects[index].dead  = 1;
        m_objects[index].count = m_free_obj;
        m_free_obj             = index;
        --m_obj_count;
    }

    bool check_count(s32 const* checks, s32 len)
    {
        bool ok = (m_obj_count == len);
        for (int i = 0; i < len && ok; ++i)
        {
            ok = ok && (m_objects[i].count == checks[i]);
        }
        return ok;
    }

    s32      m_free_obj   = EMAX_OBJECTS;
    s32      m_obj_count  = 0;
    s32      m_obj_count2 = 0;
    object_t m_objects[EMAX_OBJECTS];
};

// Object that is connected to the spatial hashgrid entity
class my_update_handler_t final : public nhshg::update_func_t
{
public:
    objects_t* m_objects;

    void update(nhshg::index_t begin, nhshg::index_t end, nhshg::entity_t* e, nhshg::index_t const* ref, nhshg::hshg_t* hshg) override final
    {
        for (nhshg::index_t i = begin; i < end; ++i)
        {
            if (m_objects->m_objects[ref[i]].remove)
            {
                nhshg::hshg_remove(hshg, i);
                m_objects->release(ref[i]);
            }
            ++update_count;
        }
    }

    void reset() { update_count = 0; }

    s32 update_count = 0;
};

class my_collision_handler_t final : public nhshg::collide_func_t
{
public:
    objects_t* m_objects;

    void collide(const nhshg::entity_t* e1, nhshg::index_t e1_ref, const nhshg::entity_t* e2, nhshg::index_t e2_ref) override final
    {
        const float dx = e1->x - e2->x;
        const float dy = e1->y - e2->y;
        const float dz = e1->z - e2->z;
        const float sr = e1->r + e2->r;

        if (dx * dx + dy * dy + dz * dz <= sr * sr)
        {
            m_objects->m_objects[e1_ref].count += 1;
            m_objects->m_objects[e2_ref].count += 1;
            collide_count += 1;
        }
    }

    void reset() { collide_count = 0; }

    s32 collide_count = 0;
};

UNITTEST_SUITE_BEGIN(test_hierarchical_spatial_hashgrid)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        static objects_t              s_objects;
        static my_update_handler_t    s_update_handler;
        static my_collision_handler_t s_collision_handler;

        UNITTEST_FIXTURE_SETUP()
        {
            s_objects.init();
            s_update_handler.reset();
            s_collision_handler.reset();
            s_update_handler.m_objects    = &s_objects;
            s_collision_handler.m_objects = &s_objects;
        }

        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(create_destroy)
        {
            for (u32 i = 1; i <= 128; i = i << 1)
            {
                for (u32 s = 1; s <= 128; s = s << 1)
                {
                    nhshg::hshg_t* hshg = nhshg::hshg_create(Allocator, i, s, 32);
                    CHECK_NOT_NULL(hshg);
                    nhshg::hshg_free(hshg);
                }
            }
        }

        UNITTEST_TEST(create_insert_destroy)
        {
            nhshg::hshg_t* hshg = nhshg::hshg_create(Allocator, 32, 32, 32);
            CHECK_NOT_NULL(hshg);

            nhshg::index_t entity_index = nhshg::hshg_insert(hshg, 0.0f, 0.0f, 0.0f, 1.0f, 0);
            CHECK_NOT_EQUAL(nhshg::c_invalid_index, entity_index);

            nhshg::hshg_free(hshg);
        }

        static bool insert_object(nhshg::hshg_t * hshg, f32 x, f32 y, f32 z, f32 r)
        {
            s32            index        = s_objects.get();
            nhshg::index_t entity_index = nhshg::hshg_insert(hshg, x, y, z, r, index);
            return entity_index != nhshg::c_invalid_index;
        }

        static bool do_check_count(s32 const* checks, s32 len) { return s_objects.check_count(checks, len); }

        static inline bool check_count(s32 a)
        {
            s32 checks[] = {a};
            return do_check_count(checks, sizeof(checks) / sizeof(checks[0]));
        }

        static inline bool check_count(s32 a, s32 b)
        {
            s32 checks[] = {a, b};
            return do_check_count(checks, sizeof(checks) / sizeof(checks[0]));
        }

        static inline bool check_count(s32 a, s32 b, s32 c)
        {
            s32 checks[] = {a, b, c};
            return do_check_count(checks, sizeof(checks) / sizeof(checks[0]));
        }

        static s32 do_check_collisions(nhshg::hshg_t * hshg)
        {
            s_collision_handler.reset();

            nhshg::hshg_collide(hshg, &s_collision_handler);
            return s_collision_handler.collide_count;
        }

        static void do_remove_update(nhshg::hshg_t * hshg)
        {
            s_update_handler.reset();

            for (s32 i = 0; i < s_objects.m_obj_count; ++i)
            {
                s_objects.m_objects[i].remove = true;
            }

            nhshg::hshg_update(hshg, &s_update_handler);
        }

        UNITTEST_TEST(insert)
        {
            nhshg::hshg_t* hshg = nhshg::hshg_create(Allocator, 32, 32, 32);
            CHECK_NOT_NULL(hshg);

            s_objects.reset();

            CHECK_TRUE(insert_object(hshg, 0.0f, 0.0f, 0.0f, 1.0f));
            CHECK_EQUAL(0, do_check_collisions(hshg));
            CHECK_TRUE(check_count(0))

            CHECK_TRUE(insert_object(hshg, 0.0f, 5.0f, 0.0f, 3.0f));
            CHECK_EQUAL(0, do_check_collisions(hshg));
            CHECK_TRUE(check_count(0, 0))

            CHECK_TRUE(insert_object(hshg, 2.0f, 1.0f, 2.0f, 2.0f));
            CHECK_EQUAL(2, do_check_collisions(hshg));
            CHECK_TRUE(check_count(1, 1, 2))

            nhshg::hshg_free(hshg);
        }

        UNITTEST_TEST(insert3_update_remove3)
        {
            nhshg::hshg_t* hshg = nhshg::hshg_create(Allocator, 32, 32, 32);
            CHECK_NOT_NULL(hshg);

            s_objects.reset();

            CHECK_TRUE(insert_object(hshg, 0.0f, 0.0f, 0.0f, 1.0f));
            CHECK_TRUE(insert_object(hshg, 0.0f, 5.0f, 0.0f, 3.0f));
            CHECK_TRUE(insert_object(hshg, 2.0f, 1.0f, 2.0f, 2.0f));

            do_remove_update(hshg);

            nhshg::hshg_free(hshg);
        }
    }
}
UNITTEST_SUITE_END
