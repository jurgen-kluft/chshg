#include "cbase/c_allocator.h"
#include "cbase/c_debug.h"
#include "cbase/c_integer.h"
#include "cbase/c_float.h"
#include "cbase/c_memory.h"
#include "chshg/c_hierarchical_spatial_hashgrid.h"

namespace ncore
{
    namespace nhshg
    {
        struct grid_t
        {
            grid_t();
            grid_t(index_t* const _cells, const cell_t _cells_side);

            DCORE_CLASS_PLACEMENT_NEW_DELETE

            index_t* const  m_cells;
            cell_t const    m_cells_side;
            cell_sq_t const m_cells_sq;
            cell_t const    m_cells_mask;   // for masking index_t to wrap around grid
            u8 const        m_cells2d_log;  // number of bits to shift y
            u8 const        m_cells3d_log;  // number of bits to shift z
            u8              m_shift;
            f32 const       m_inverse_cell_size;
            index_t         m_entities_len;
        };

        // A cell will hold a doubly linked list of entities, this is a part of an entity
        // used as a node in the doubly linked list.
        struct entity_node_t
        {
            index_t m_next;
            index_t m_prev;
        };

        class hshg_t
        {
        public:
            hshg_t();
            hshg_t(index_t* _cells, grid_t* _grids, u32 _size, cell_sq_t _cells_len, u8 _grids_len, cell_sq_t _grid_size, u32 _max_entities);

            DCORE_CLASS_PLACEMENT_NEW_DELETE

            //
            //  Creates a new HSHG.
            //
            // \param side; the number of cells on the smallest grid's edge (must be a power of two!)
            // \param size; smallest cell size in world units, e.g. 8 = 8 meters (must be a power of two!)
            //
            inline u8 calling() const { return m_bupdating | m_bcolliding | m_bquerying; }

            inline void set_updating(bool value) { m_bupdating = value; }
            inline void set_colliding(bool value) { m_bcolliding = value; }
            inline void set_querying(bool value) { m_bquerying = value; }
            inline void set_removed(bool value) { m_bremoved = value; }

            inline bool is_updating() const { return m_bupdating; }
            inline bool is_colliding() const { return m_bcolliding; }
            inline bool is_querying() const { return m_bquerying; }
            inline bool is_removed() const { return m_bremoved; }

            void update_cache();

            inline u8 get_grid(const f32 r) const
            {
                const u32 rounded = r + r;
                if (rounded < m_cell_size)
                {
                    return 0;
                }
                const u8 grid = m_cell_log - math::countLeadingZeros(rounded) + 1;
                return math::min(grid, (u8)(m_grids_len - 1));
            }

            index_t create_entity()
            {
                if (m_entities_used < m_entities_max)
                    return m_entities_used++;

                // No more free entities available
                return c_invalid_index;
            }

            void insert_into_grid(const index_t entity_id);
            void detach_from_grid(index_t entity_id);

            void destroy_entity(index_t entity_id)
            {
                m_entities_used--;
                m_free_entities.set_free(entity_id);
                m_used_entities.set_used(entity_id);
            }

            entity_t*      m_entities;       // entities * 16 bytes
            entity_node_t* m_entities_node;  // entities * 8 bytes
            cell_sq_t*     m_entities_cell;  // entities * 4 bytes
            u8*            m_entities_grid;  // entities * 1 byte
            index_t*       m_entities_ref;   // entities * 4 bytes

            index_t* const m_cells;

            u8 const m_cell_log;
            u8 const m_grids_len;

            u8 m_bupdating : 1;
            u8 m_bcolliding : 1;
            u8 m_bquerying : 1;
            u8 m_bremoved : 1;

            u32 m_old_cache;
            u32 m_new_cache;

            cell_sq_t const m_grid_size;
            f32 const       m_inverse_grid_size;
            cell_sq_t const m_cells_len;
            u32 const       m_cell_size;

            binmap_t      m_free_entities;
            binmap_t      m_used_entities;
            index_t       m_entities_used;
            index_t const m_entities_max;

            grid_t*  m_grids;
            alloc_t* m_allocator;
        };

        static u8 compute_max_grids(cell_t side)
        {
            u8 grids_len = 0;
            do
            {
                ++grids_len;
                side >>= 1;
            } while (side >= 2);
            return grids_len;
        }

        static cell_sq_t compute_max_cells(cell_t side)
        {
            cell_sq_t cells_len = 0;
            do
            {
                const cell_sq_t cell_sq = cells_len + (cell_sq_t)side * side * side;
                ASSERT(cell_sq > cells_len && "cell_sq_t must be set to a wider data type");
                cells_len = cell_sq;
                side >>= 1;
            } while (side >= 2);
            return cells_len;
        }

        static cell_t grid_get_cell_1d(const grid_t* const grid, const f32 x)
        {
            const cell_t cell = math::abs(x) * grid->m_inverse_cell_size;
            if (cell & grid->m_cells_side)
            {
                return grid->m_cells_mask - (cell & grid->m_cells_mask);
            }
            return cell & grid->m_cells_mask;
        }

        static cell_sq_t grid_get_idx(const grid_t* const grid, const cell_sq_t x, const cell_sq_t y, const cell_sq_t z) { return x | (y << grid->m_cells2d_log) | (z << grid->m_cells3d_log); }
        static cell_t    idx_get_x(const grid_t* const grid, const cell_sq_t cell) { return cell & grid->m_cells_mask; }
        static cell_t    idx_get_y(const grid_t* const grid, const cell_sq_t cell) { return (cell >> grid->m_cells2d_log) & grid->m_cells_mask; }
        static cell_t    idx_get_z(const grid_t* const grid, const cell_sq_t cell) { return cell >> grid->m_cells3d_log; }

        static cell_sq_t grid_get_cell(const grid_t* const grid, const f32 x, const f32 y, const f32 z)
        {
            const cell_t cell_x = grid_get_cell_1d(grid, x);
            const cell_t cell_y = grid_get_cell_1d(grid, y);
            const cell_t cell_z = grid_get_cell_1d(grid, z);

            return grid_get_idx(grid, cell_x, cell_y, cell_z);
        }

        grid_t::grid_t()
            : m_cells(nullptr)
            , m_cells_side(0)
            , m_cells_sq(0)
            , m_cells_mask(0)
            , m_cells2d_log(0)
            , m_cells3d_log(0)
            , m_shift(0)
            , m_inverse_cell_size(0)
            , m_entities_len(0)
        {
        }

        grid_t::grid_t(index_t* const _cells_array, const cell_t _cells_side)
            : m_cells(_cells_array)
            , m_cells_side(_cells_side)
            , m_cells_sq((cell_sq_t)_cells_side * _cells_side)
            , m_cells_mask(_cells_side - 1)
            , m_cells2d_log(math::countTrailingZeros(_cells_side) << 0)
            , m_cells3d_log(math::countTrailingZeros(_cells_side) << 1)
            , m_shift(0)
            , m_inverse_cell_size((f32)1.0 / _cells_side)
            , m_entities_len(0)
        {
        }

        hshg_t::hshg_t()
            : m_entities(nullptr)
            , m_entities_node(nullptr)
            , m_entities_grid(nullptr)
            , m_entities_ref(nullptr)
            , m_cells(nullptr)
            , m_cell_log(0)
            , m_grids_len(0)
            , m_bupdating(0)
            , m_bcolliding(0)
            , m_bquerying(0)
            , m_bremoved(0)
            , m_old_cache(0)
            , m_new_cache(0)
            , m_grid_size(0)
            , m_inverse_grid_size(0)
            , m_cells_len(0)
            , m_cell_size(0)
            , m_entities_used(0)
            , m_entities_max(0)
            , m_grids(nullptr)
        {
        }

        hshg_t::hshg_t(index_t* _cells, grid_t* _grids, u32 _size, cell_sq_t _cells_len, u8 _grids_len, cell_sq_t _grid_size, u32 _max_entities)
            : m_entities(nullptr)
            , m_entities_node(nullptr)
            , m_entities_grid(nullptr)
            , m_entities_ref(nullptr)
            , m_cells(_cells)
            , m_cell_log(31 - math::countTrailingZeros(_size))
            , m_grids_len(_grids_len)
            , m_bupdating(0)
            , m_bcolliding(0)
            , m_bquerying(0)
            , m_bremoved(0)
            , m_old_cache(0)
            , m_new_cache(0)
            , m_grid_size(_grid_size)
            , m_inverse_grid_size((f32)1.0 / _grid_size)
            , m_cells_len(_cells_len)
            , m_cell_size(_size)
            , m_entities_used(0)
            , m_entities_max(_max_entities)
            , m_grids(_grids)
        {
        }

        hshg_t* hshg_create(alloc_t* allocator, const cell_t _side, const u32 _size, const u32 _max_entities)
        {
            ASSERTS(math::ispo2(_side), "_side must be a power of 2!");
            ASSERTS(math::ispo2(_size), "_size must be a power of 2!");

            const cell_sq_t cells_len = compute_max_cells(_side);
            index_t* const  cells     = (index_t*)allocator->allocate(sizeof(index_t) * cells_len);
            if (cells == nullptr)
            {
                return nullptr;
            }
            nmem::memset(cells, 0xFFFFFFFF, sizeof(index_t) * cells_len);

            const u8        grids_len = compute_max_grids(_side);
            const cell_sq_t grid_size = (cell_sq_t)_side * _size;
            grid_t*         grids     = (grid_t*)allocator->allocate(sizeof(grid_t) * grids_len);
            if (grids == nullptr)
            {
                allocator->deallocate(cells);
                return nullptr;
            }

            void*         instance_mem = allocator->allocate(sizeof(hshg_t));
            hshg_t* const hshg         = new (instance_mem) hshg_t(cells, grids, _size, cells_len, grids_len, grid_size, _max_entities);
            if (hshg == nullptr)
            {
                allocator->deallocate(cells);
                allocator->deallocate(grids);
                return nullptr;
            }

            hshg->m_allocator     = allocator;
            hshg->m_entities      = (entity_t*)allocator->allocate(sizeof(entity_t) * _max_entities);
            hshg->m_entities_node = (entity_node_t*)allocator->allocate(sizeof(entity_node_t) * _max_entities);
            hshg->m_entities_cell = (cell_sq_t*)allocator->allocate(sizeof(cell_sq_t) * _max_entities);
            hshg->m_entities_grid = (u8*)allocator->allocate(sizeof(u8) * _max_entities);
            hshg->m_entities_ref  = (index_t*)allocator->allocate(sizeof(index_t) * _max_entities);
            if (hshg->m_entities == nullptr || hshg->m_entities_node == nullptr || hshg->m_entities_grid == nullptr)
            {
                hshg_free(hshg);
                return nullptr;
            }

            hshg->m_free_entities.init_all_used(_max_entities, allocator);
            hshg->m_used_entities.init_all_free(_max_entities, allocator);

            index_t idx   = 0;
            u32     isize = _size;
            cell_t  iside = _side;

            // initialize array of grid_t
            for (u8 i = 0; i < grids_len; ++i)
            {
                void* gridmem = hshg->m_grids + i;
                new (gridmem) grid_t(hshg->m_cells + idx, iside);
                idx += (cell_sq_t)iside * iside * iside;
                iside >>= 1;
                isize <<= 1;
            }

            return hshg;
        }

        void hshg_free(hshg_t* const hshg)
        {
            hshg->m_allocator->deallocate(hshg->m_entities);
            hshg->m_allocator->deallocate(hshg->m_entities_node);
            hshg->m_allocator->deallocate(hshg->m_entities_cell);
            hshg->m_allocator->deallocate(hshg->m_entities_grid);
            hshg->m_allocator->deallocate(hshg->m_entities_ref);

            hshg->m_allocator->deallocate(hshg->m_cells);
            hshg->m_allocator->deallocate(hshg->m_grids);

            hshg->m_free_entities.release(hshg->m_allocator);
            hshg->m_used_entities.release(hshg->m_allocator);

            hshg->m_allocator->deallocate(hshg);
        }

        int_t hshg_memory_usage(const cell_t side, const index_t max_entities)
        {
            const int_t entities = (sizeof(entity_t) + sizeof(entity_node_t) + sizeof(cell_sq_t) + sizeof(u8) + sizeof(index_t)) * max_entities;
            const int_t cells    = sizeof(index_t) * compute_max_cells(side);
            const int_t grids    = sizeof(grid_t) * compute_max_grids(side);
            const int_t hshg     = sizeof(hshg_t);
            return entities + cells + grids + hshg;
        }

        void hshg_t::insert_into_grid(const index_t idx)
        {
            entity_t* const      entity      = m_entities + idx;
            entity_node_t* const entity_node = m_entities_node + idx;

            grid_t* const grid = m_grids + m_entities_grid[idx];

            m_entities_cell[idx] = grid_get_cell(grid, entity->x, entity->y, entity->z);
            index_t* const cell  = grid->m_cells + m_entities_cell[idx];

            entity_node->m_next = *cell;
            if (entity_node->m_next != c_invalid_index)
            {
                m_entities_node[entity_node->m_next].m_prev = idx;
            }

            entity_node->m_prev = c_invalid_index;
            *cell               = idx;

            if (grid->m_entities_len == 0)
            {
                m_new_cache |= ((u32)1 << m_entities_grid[idx]);
            }

            ++grid->m_entities_len;
        }

        bool hshg_insert(hshg_t* const hshg, const f32 x, const f32 y, const f32 z, const f32 r, const index_t ref)
        {
            ASSERT(!hshg->calling() && "insert() may not be called from any callback");

            const index_t idx = hshg->create_entity();
            if (idx == c_invalid_index)
                return false;

            entity_node_t* const ent2 = hshg->m_entities_node + idx;
            ent2->m_next              = c_invalid_index;
            ent2->m_prev              = c_invalid_index;

            entity_t* const ent = hshg->m_entities + idx;
            ent->x              = x;
            ent->y              = y;
            ent->z              = z;
            ent->r              = r;

            hshg->m_entities_cell[idx] = 0;
            hshg->m_entities_grid[idx] = hshg->get_grid(r);
            hshg->m_entities_ref[idx]  = ref;

            hshg->insert_into_grid(idx);
            return true;
        }

        // detach_from_grid an entity from the grid, to be re-inserted again in another cell
        void hshg_t::detach_from_grid(index_t entity_id)
        {
            index_t const        idx         = entity_id;
            entity_t* const      entity      = m_entities + idx;
            entity_node_t* const entity_node = m_entities_node + idx;
            u8 const             entity_grid = m_entities_grid[idx];

            grid_t* const grid = m_grids + entity_grid;

            if (entity_node->m_next != c_invalid_index)
            {
                m_entities_node[entity_node->m_next].m_prev = entity_node->m_prev;
            }
            if (entity_node->m_prev != c_invalid_index)
            {
                m_entities_node[entity_node->m_prev].m_next = entity_node->m_next;
            }
            else
            {
                // we are at the head of the list, so update the grid cell
                grid->m_cells[m_entities_cell[idx]] = entity_node->m_next;
            }

            --grid->m_entities_len;
            if (grid->m_entities_len == 0)
            {
                // There are no more entities in the grid, so we need to update the cache
                m_new_cache ^= (u32)1 << entity_grid;
            }
        }

        void hshg_remove(hshg_t* hshg, index_t e)
        {
            ASSERT(hshg->is_updating() && "remove() may only be called from within update()");
            hshg->set_removed(true);
            hshg->detach_from_grid(e);
            hshg->destroy_entity(e);
        }

        void hshg_move(hshg_t* hshg, index_t e)
        {
            ASSERT(hshg->is_updating() && "move() may only be called from within hshg.update()");

            const grid_t* const grid     = hshg->m_grids + hshg->m_entities_grid[e];
            entity_t* const     entity   = hshg->m_entities + e;
            const cell_sq_t     new_cell = grid_get_cell(grid, entity->x, entity->y, entity->z);

            if (new_cell != hshg->m_entities_cell[e])
            {
                hshg->detach_from_grid(e);
                hshg->insert_into_grid(e);
            }
        }

        void hshg_resize(hshg_t* hshg, index_t e)
        {
            ASSERT(hshg->is_updating() && "resize() may only be called from within hshg.update()");

            entity_t* const entity   = hshg->m_entities + e;
            const u8        new_grid = hshg->get_grid(entity->r);

            if (hshg->m_entities_grid[e] != new_grid)
            {
                hshg->detach_from_grid(e);
                hshg->m_entities_grid[e] = new_grid;
                hshg->insert_into_grid(e);
            }
        }

        static void swap_entity(hshg_t* const hshg, index_t _free_entity, index_t _used_entity)
        {
            entity_t* const      used_entity     = hshg->m_entities + _used_entity;
            entity_node_t* const used_entity2    = hshg->m_entities_node + _used_entity;
            index_t* const       used_entity_ref = hshg->m_entities_ref + _used_entity;

            // swap entity data, also make sure we remove and insert_into_grid the entity into the cell
            index_t* cell = hshg->m_cells + hshg->m_entities_cell[_used_entity];
            if (*cell == _used_entity)
            {
                *cell = _free_entity;
            }

            // remove the used entity from the doubly linked list and insert the free entity
            hshg->m_entities_node[used_entity2->m_prev].m_next = _free_entity;

            entity_t* const      free_entity  = hshg->m_entities + _free_entity;
            entity_node_t* const free_entity2 = hshg->m_entities_node + _free_entity;
            free_entity2->m_prev              = used_entity2->m_prev;
            free_entity2->m_next              = used_entity2->m_next;
            free_entity->x                    = used_entity->x;
            free_entity->y                    = used_entity->y;
            free_entity->z                    = used_entity->z;
            free_entity->r                    = used_entity->r;

            hshg->m_entities_cell[_free_entity] = hshg->m_entities_cell[_used_entity];
            hshg->m_entities_ref[_free_entity]  = hshg->m_entities_ref[_used_entity];
            hshg->m_entities_grid[_free_entity] = hshg->m_entities_grid[_used_entity];

            // Mark the used entity as free and the free entity as used
            hshg->m_free_entities.set_free(_used_entity);
            hshg->m_used_entities.set_used(_free_entity);
        }

        void hshg_update(hshg_t* const hshg, update_func_t* const func)
        {
            ASSERT(!hshg->calling() && "update() may not be called from any callback");
            hshg->set_updating(true);

            // Since the entities that are active are in a contiguous array, we can hand them off to the handler in one go.
            func->update(0, hshg->m_entities_used, hshg->m_entities, &hshg->m_entities_ref[0], hshg);

            // process the free entities and swap any free entity with an entity at the top of the array.
            // this means that after this step the array of entities that are valid are contiguous.
            s32 free_entity = hshg->m_free_entities.find_upper_and_set();
            while (free_entity >= 0)
            {
                if (hshg->m_entities_used > 0)
                {
                    --hshg->m_entities_used;
                    s32 used_entity = hshg->m_used_entities.find_upper();
                    if (free_entity < used_entity)
                    {
                        swap_entity(hshg, free_entity, used_entity);
                    }
                }

                // on to the next free entity
                free_entity = hshg->m_free_entities.find_upper_and_set();
            }

            hshg->set_removed(false);
            hshg->set_updating(false);
        }

        void hshg_update_multithread(hshg_t* const hshg, const u8 threads, const u8 idx, multi_threaded_update_func_t* const handler)
        {
            const index_t used  = hshg->m_entities_used - 1;
            const index_t div   = used / threads;
            const index_t start = div * idx + 1;
            const index_t end   = div + (idx + 1 == threads ? (used % threads) : 0) + 1;

            // Since the entities that are active are in a contiguous array, we can hand them off to the handler in one go.
            handler->update(start, end, hshg->m_entities, &hshg->m_entities_ref[start], hshg);
        }

        void hshg_t::update_cache()
        {
            if (this->m_old_cache == this->m_new_cache)
            {
                return;
            }

            this->m_old_cache = this->m_new_cache;

            grid_t*             old_grid;
            const grid_t* const grid_max = this->m_grids + this->m_grids_len;

            for (old_grid = this->m_grids; old_grid != grid_max; ++old_grid)
            {
                old_grid->m_shift = 0;
            }

            old_grid = this->m_grids;

            while (1)
            {
                if (old_grid == grid_max)
                    return;
                if (old_grid->m_entities_len != 0)
                    break;

                ++old_grid;
            }

            grid_t* new_grid;

            u8 shift = 1;

            for (new_grid = old_grid + 1; new_grid != grid_max; ++new_grid)
            {
                if (new_grid->m_entities_len == 0)
                {
                    ++shift;
                    continue;
                }

                old_grid->m_shift = shift;
                old_grid          = new_grid;
                shift             = 1;
            }
        }

        static void inline loop_over(hshg_t* hshg, const index_t ref, const entity_t* entity, const index_t from, collide_func_t* handler)
        {
            index_t n = from;
            while (n != c_invalid_index)
            {
                handler->collide(entity, ref, &hshg->m_entities[n], hshg->m_entities_ref[n]);
                n = hshg->m_entities_node[n].m_next;
            }
        }

        void hshg_collide(hshg_t* const hshg, collide_func_t* const handler)
        {
            ASSERT(!hshg->calling() && "collide() may not be called from any callback");
            hshg->set_colliding(true);

            hshg->update_cache();

            for (index_t i = 0; i < hshg->m_entities_used; ++i)
            {
                const entity_t*      entity      = hshg->m_entities + i;
                const entity_node_t* entity_node = hshg->m_entities_node + i;
                const cell_sq_t      entity_cell = hshg->m_entities_cell[i];

                const grid_t* grid = hshg->m_grids + hshg->m_entities_grid[i];

                cell_t cell_x = idx_get_x(grid, entity_cell);
                cell_t cell_y = idx_get_y(grid, entity_cell);
                cell_t cell_z = idx_get_z(grid, entity_cell);
                if (cell_z != 0)
                {
                    if (cell_y != 0)
                    {
                        const index_t* const cell = grid->m_cells + (entity_cell - grid->m_cells_sq - grid->m_cells_side);

                        if (cell_x != 0)
                        {
                            loop_over(hshg, i, entity, *(cell - 1), handler);
                        }

                        loop_over(hshg, i, entity, *cell, handler);

                        if (cell_x != grid->m_cells_mask)
                        {
                            loop_over(hshg, i, entity, *(cell + 1), handler);
                        }
                    }

                    {
                        const index_t* const cell = grid->m_cells + (entity_cell - grid->m_cells_sq);

                        if (cell_x != 0)
                        {
                            loop_over(hshg, i, entity, *(cell - 1), handler);
                        }

                        loop_over(hshg, i, entity, *cell, handler);

                        if (cell_x != grid->m_cells_mask)
                        {
                            loop_over(hshg, i, entity, *(cell + 1), handler);
                        }
                    }

                    if (cell_y != grid->m_cells_mask)
                    {
                        const index_t* const cell = grid->m_cells + (entity_cell - grid->m_cells_sq + grid->m_cells_side);

                        if (cell_x != 0)
                        {
                            loop_over(hshg, i, entity, *(cell - 1), handler);
                        }

                        loop_over(hshg, i, entity, *cell, handler);

                        if (cell_x != grid->m_cells_mask)
                        {
                            loop_over(hshg, i, entity, *(cell + 1), handler);
                        }
                    }
                }
                loop_over(hshg, i, entity, entity_node->m_next, handler);

                if (cell_x != grid->m_cells_mask)
                {
                    loop_over(hshg, i, entity, grid->m_cells[entity_cell + 1], handler);
                }

                if (cell_y != grid->m_cells_mask)
                {
                    const index_t* const cell = grid->m_cells + (entity_cell + grid->m_cells_side);

                    if (cell_x != 0)
                    {
                        loop_over(hshg, i, entity, *(cell - 1), handler);
                    }

                    loop_over(hshg, i, entity, *cell, handler);

                    if (cell_x != grid->m_cells_mask)
                    {
                        loop_over(hshg, i, entity, *(cell + 1), handler);
                    }
                }

                while (grid->m_shift)
                {
                    cell_x >>= grid->m_shift;
                    cell_y >>= grid->m_shift;
                    cell_z >>= grid->m_shift;

                    grid += grid->m_shift;

                    const cell_t min_cell_x = cell_x != 0 ? cell_x - 1 : 0;
                    const cell_t min_cell_y = cell_y != 0 ? cell_y - 1 : 0;
                    const cell_t min_cell_z = cell_z != 0 ? cell_z - 1 : 0;

                    const cell_t max_cell_x = cell_x != grid->m_cells_mask ? cell_x + 1 : cell_x;
                    const cell_t max_cell_y = cell_y != grid->m_cells_mask ? cell_y + 1 : cell_y;
                    const cell_t max_cell_z = cell_z != grid->m_cells_mask ? cell_z + 1 : cell_z;

                    for (cell_t cur_z = min_cell_z; cur_z <= max_cell_z; ++cur_z)
                    {
                        for (cell_t cur_y = min_cell_y; cur_y <= max_cell_y; ++cur_y)
                        {
                            for (cell_t cur_x = min_cell_x; cur_x <= max_cell_x; ++cur_x)
                            {
                                const cell_t cell = grid_get_idx(grid, cur_x, cur_y, cur_z);
                                loop_over(hshg, i, entity, grid->m_cells[cell], handler);
                            }
                        }
                    }
                }
            }

            hshg->set_colliding(false);
        }

        struct cell_range_t
        {
            cell_t start;
            cell_t end;
        };

        static cell_range_t map_pos(const hshg_t* const hshg, const f32 _x1, const f32 _x2)
        {
            f32 x1;
            f32 x2;

            if (_x1 < 0)
            {
                const f32 shift = (((cell_t)(-_x1 * hshg->m_inverse_grid_size) << 1) + 2) * hshg->m_grid_size;

                x1 = _x1 + shift;
                x2 = _x2 + shift;
            }
            else
            {
                x1 = _x1;
                x2 = _x2;
            }

            cell_t folds = (x2 - (cell_t)(x1 * hshg->m_inverse_grid_size) * hshg->m_grid_size) * hshg->m_inverse_grid_size;

            const grid_t* const grid = hshg->m_grids;

            cell_t start;
            cell_t end;
            switch (folds)
            {
                case 0:
                {
                    const cell_t cell = grid_get_cell_1d(grid, x1);

                    end   = grid_get_cell_1d(grid, x2);
                    start = math::min(cell, end);
                    end   = math::max(cell, end);

                    break;
                }
                case 1:
                {
                    const cell_t cell = math::abs(x1) * grid->m_inverse_cell_size;

                    end = grid_get_cell_1d(grid, x2);

                    if (cell & grid->m_cells_side)
                    {
                        start = 0;
                        end   = math::max(grid->m_cells_mask - (cell & grid->m_cells_mask), end);
                    }
                    else
                    {
                        start = math::min(cell & grid->m_cells_mask, end);
                        end   = grid->m_cells_mask;
                    }

                    break;
                }
                default:
                {
                    start = 0;
                    end   = grid->m_cells_mask;

                    break;
                }
            }

            return {start, end};
        }

        static void query_common(const hshg_t* const hshg, const f32 x1, const f32 y1, const f32 z1, const f32 x2, const f32 y2, const f32 z2, query_func_t* const handler)
        {
            ASSERT(x1 <= x2);
            ASSERT(y1 <= y2);
            ASSERT(z1 <= z2);

            cell_range_t x = map_pos(hshg, x1, x2);
            cell_range_t y = map_pos(hshg, y1, y2);
            cell_range_t z = map_pos(hshg, z1, z2);

            const grid_t*       grid     = hshg->m_grids;
            const grid_t* const grid_max = hshg->m_grids + hshg->m_grids_len;

            u8 shift = 0;

            while (1)
            {
                if (grid == grid_max)
                {
                    return;
                }

                if (grid->m_entities_len != 0)
                {
                    break;
                }

                ++grid;
                ++shift;
            }

            x.start >>= shift;
            y.start >>= shift;
            z.start >>= shift;

            x.end >>= shift;
            y.end >>= shift;
            z.end >>= shift;

            while (1)
            {
                const cell_t s_x = x.start != 0 ? x.start - 1 : 0;
                const cell_t s_y = y.start != 0 ? y.start - 1 : 0;
                const cell_t s_z = z.start != 0 ? z.start - 1 : 0;

                const cell_t e_x = x.end != grid->m_cells_mask ? x.end + 1 : x.end;
                const cell_t e_y = y.end != grid->m_cells_mask ? y.end + 1 : y.end;
                const cell_t e_z = z.end != grid->m_cells_mask ? z.end + 1 : z.end;

                for (cell_t z = s_z; z <= e_z; ++z)
                {
                    for (cell_t y = s_y; y <= e_y; ++y)
                    {
                        for (cell_t x = s_x; x <= e_x; ++x)
                        {
                            const cell_sq_t cell = grid_get_idx(grid, x, y, z);

                            index_t entity_idx = grid->m_cells[cell];
                            while (entity_idx != c_invalid_index)
                            {
                                const entity_t* const entity = hshg->m_entities + entity_idx;
                                if ((entity->x + entity->r >= x1 && entity->x - entity->r <= x2) && entity->y + entity->r >= y1 && entity->y - entity->r <= y2 && entity->z + entity->r >= z1 && entity->z - entity->r <= z2)
                                {
                                    handler->query(entity, hshg->m_entities_ref[entity_idx]);
                                }

                                const entity_node_t* const entity_node = hshg->m_entities_node + entity_idx;
                                entity_idx                             = entity_node->m_next;
                            }
                        }
                    }
                }

                if (grid->m_shift)
                {
                    x.start >>= grid->m_shift;
                    y.start >>= grid->m_shift;
                    z.start >>= grid->m_shift;

                    x.end >>= grid->m_shift;
                    y.end >>= grid->m_shift;
                    z.end >>= grid->m_shift;

                    grid += grid->m_shift;
                }
                else
                {
                    break;
                }
            }
        }

        void hshg_query(hshg_t* const hshg, const f32 x1, const f32 y1, const f32 z1, const f32 x2, const f32 y2, const f32 z2, query_func_t* const handler)
        {
            ASSERT((!hshg->is_updating() || (hshg->is_updating() && !hshg->is_removed())) &&
                   "remove() and query() can't be mixed in the same "
                   "update() tick, consider calling update() twice");

#ifndef HSHG_NDEBUG
            const bool old_querying = hshg->is_querying();
#endif
            hshg->set_querying(true);
            hshg->update_cache();
            query_common(hshg, x1, y1, z1, x2, y2, z2, handler);
            hshg->set_querying(old_querying);
        }

        void hshg_query_multithread(hshg_t* const hshg, const f32 x1, const f32 y1, const f32 z1, const f32 x2, const f32 y2, const f32 z2, query_func_t* const handler)
        {
            ASSERT(hshg->m_old_cache == hshg->m_new_cache &&
                   "You modified an entity's radius. "
                   "Call update_cache() before any query_multithread().");

            query_common(hshg, x1, y1, z1, x2, y2, z2, handler);
        }

        void hshg_optimize(hshg_t* const hshg)
        {
            ASSERT(!hshg->calling() && "hshg_optimize() may not be called from any callback");

            entity_t* const      entities      = (entity_t*)hshg->m_allocator->allocate(sizeof(entity_t) * hshg->m_entities_max);
            entity_node_t* const entities_node = (entity_node_t*)hshg->m_allocator->allocate(sizeof(entity_node_t) * hshg->m_entities_max);
            cell_sq_t*           entities_cell = (cell_sq_t*)hshg->m_allocator->allocate(sizeof(cell_sq_t) * hshg->m_entities_max);
            u8*                  entities_grid = (u8*)hshg->m_allocator->allocate(sizeof(u8) * hshg->m_entities_max);
            index_t*             entities_ref  = (index_t*)hshg->m_allocator->allocate(sizeof(index_t) * hshg->m_entities_max);

            if (entities == nullptr)
                return;

            index_t  new_entity_idx = 0;
            index_t* cell           = hshg->m_cells;

            for (cell_sq_t i = 0; i < hshg->m_cells_len; ++i)
            {
                index_t entity_idx = *cell;
                if (entity_idx == c_invalid_index)
                {
                    ++cell;
                    continue;
                }

                *cell = new_entity_idx;
                ++cell;

                while (1)
                {
                    entity_t* const new_entity    = entities + new_entity_idx;
                    *new_entity                   = hshg->m_entities[entity_idx];
                    entities_cell[new_entity_idx] = hshg->m_entities_cell[entity_idx];
                    entities_grid[new_entity_idx] = hshg->m_entities_grid[entity_idx];
                    entities_ref[new_entity_idx]  = hshg->m_entities_ref[entity_idx];

                    entity_node_t const* const cur_entity_node = entities_node + new_entity_idx;
                    entity_node_t* const       new_entity_node = entities_node + new_entity_idx;
                    if (cur_entity_node->m_prev != c_invalid_index)
                    {
                        new_entity_node->m_prev = new_entity_idx - 1;
                    }
                    else
                    {
                        new_entity_node->m_prev = c_invalid_index;
                    }

                    ++new_entity_idx;

                    if (cur_entity_node->m_next == c_invalid_index)
                    {
                        new_entity_node->m_next = c_invalid_index;
                        break;
                    }

                    entity_idx              = cur_entity_node->m_next;
                    new_entity_node->m_next = new_entity_idx;
                }
            }

            hshg->m_allocator->deallocate(hshg->m_entities);
            hshg->m_allocator->deallocate(hshg->m_entities_node);
            hshg->m_allocator->deallocate(hshg->m_entities_cell);
            hshg->m_allocator->deallocate(hshg->m_entities_grid);
            hshg->m_allocator->deallocate(hshg->m_entities_ref);

            hshg->m_entities      = entities;
            hshg->m_entities_node = entities_node;
            hshg->m_entities_cell = entities_cell;
            hshg->m_entities_grid = entities_grid;
            hshg->m_entities_ref  = entities_ref;
        }
    }  // namespace nhshg

}  // namespace ncore
