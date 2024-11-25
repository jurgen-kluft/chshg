#ifndef __C_HIERARCHICAL_SPATIAL_HASHGRID_H__
#define __C_HIERARCHICAL_SPATIAL_HASHGRID_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
    #pragma once
#endif

namespace ncore
{
    class alloc_t;

    namespace nhshg
    {
        typedef u32 index_t;
        typedef u32 ref_t;
        typedef u32 cell_t;

        const index_t c_invalid_index = 0xFFFFFFFF;

        //
        // A type that will be able to hold the total number of cells in a HSHG. To get
        // an upper bound of that number, calculate:
        //
        // ( side ** dimension ) * [ 2, 1.333, 1.143 ][ dimension ]
        //
        // For 2D, you would do: side * side * 1.333.
        //
        typedef u32 cell_sq_t;

        struct entity_t
        {
            f32 x;
            f32 y;
            f32 z;
            f32 r;
        };

        class hshg_t;

        class update_func_t
        {
        public:
            virtual void update(nhshg::index_t begin, nhshg::index_t end, nhshg::entity_t* entity_array, nhshg::index_t const* ref_array, nhshg::hshg_t* hshg) = 0;
        };

        class multi_threaded_update_func_t
        {
        public:
            virtual void update(nhshg::index_t begin, nhshg::index_t end, nhshg::entity_t const* entity_array, nhshg::index_t const* ref_array, nhshg::hshg_t* hshg) = 0;
        };

        class collide_func_t
        {
        public:
            virtual void collide(nhshg::entity_t const* e1, nhshg::index_t e1_ref, entity_t const* e2, nhshg::index_t e2_ref) = 0;
        };

        class query_func_t
        {
        public:
            virtual void query(nhshg::entity_t const* e, nhshg::index_t e1_ref) = 0;
        };

        hshg_t* hshg_create(alloc_t* allocator, const cell_t side, const u32 size, const u32 max_entities);
        void    hshg_free(hshg_t* const hshg);

        void    hshg_remove(hshg_t* hshg, index_t entity_index);
        void    hshg_move(hshg_t* hshg, index_t entity_index);
        void    hshg_resize(hshg_t* hshg, index_t entity_index);
        index_t hshg_insert(hshg_t* const hshg, const f32 x, const f32 y, const f32 z, const f32 r, const index_t ref);
        void    hshg_update(hshg_t* const hshg, update_func_t* const func);
        void    hshg_update_multithread(hshg_t* const hshg, const u8 threads, const u8 idx, multi_threaded_update_func_t* const func);
        void    hshg_collide(hshg_t* const hshg, collide_func_t* const func);
        void    hshg_query(hshg_t* const hshg, const f32 min_x, const f32 min_y, const f32 min_z, const f32 max_x, const f32 max_y, const f32 max_z, query_func_t* const func);
        void    hshg_query_multithread(hshg_t* const hshg, const f32 min_x, const f32 min_y, const f32 min_z, const f32 max_x, const f32 max_y, const f32 max_z, query_func_t* const handler);
        void    hshg_optimize(hshg_t* const hshg);

        //
        // Returns the maximum amount of memory a HSHG with given parameters will use,
        // NOT including the usage of `hshg_optimize()`. If you also need to take that
        // function into consideration, double the maximum number of entities you pass
        // to this function.
        //
        // The number of entities you pass must include the zero entity as well, so
        // per one full array of entities you need to add 1. If you want to also
        // reserve memory for hshg_optimize(), then you need to both double the
        // amount of memory for entities and for the extra spot, resulting in +2.
        //
        // \param side the number of cells on the smallest grid's edge
        // \param entities_max the maximum number of entities that will ever be
        // inserted
        //
        int_t hshg_memory_usage(const cell_t side, const index_t entities_max);

    }  // namespace nhshg
}  // namespace ncore

#endif  // __C_GFX_COMMON_OFFSET_ALLOCATOR_H__
