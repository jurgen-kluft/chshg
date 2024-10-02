# chshg

3D Hierarchical Spatial HashGrid C++ library.

## Behind the scenes

This section does not document API functions, however you should read this before trying to use the library.

### Grids

A Hierarchical Spatial Hash Grid, as the name suggests, keeps track of multiple grids that are ordered in a hierarchy, from the "tightest" grid (the most number of cells, the smallest cells) to the "loosest" one (the least number of cells, the biggest cells). In this specific implementation of a HSHG, all grids must have a cell size that's a power of 2, as well as a number of cells on one side being a power of 2. Thus, every grid is a square, with square cells, and subsequent loose grids are created by taking the previous loosest grid, dividing its number of cells on one side by 2 and multiplying its cell size by 2. The new grid is then of the same size as the old one, however it has fewer cells, with the cells being bigger.

The restriction of cells and their size being a power of 2 does not impact updating or collision, while query is affected only partially. For most applications, this means constant time of computation no matter how many cells there are, with the benefit that more cells usually equals higher performance. Sadly, this is only true if not taking into account `hshg_optimize()`, which **entirely** depends on the number of cells. Even though it can decrease the time needed for collision by more than 4 times, it itself takes a big chunk of time too, scaling with a steep slope with the number of cells, even more that only powers of 2 are allowed, so the ideal number that guarantees the higher performance is basically never reachable.

All grids are precreated upon initializing a HSHG. To avoid a huge performance impact when searching for collisions or queries, because it would involve traversing possibly tens of grids, an array is employed that only holds the "active" grids, having at least one entity in them. Since that now creates a not linear environment where one grid can have 2, 4, 8, etc. times the number of cells of the other one (no longer guaranteed 2), they also keep information about how big of a leap there is between the consecutive grids in the array, to easily traverse it linearly.

### Entities

All entities have hypercube hitboxes. It doesn't matter if your shape is convex or not, you just need to provide a position and a radius that will create a hypercube containing your whole shape inside, where position is the center of the hypercube, and radius is half of an edge's length. If your shape is exceptionally "long" in one direction, which would result in a pretty huge hypercube, you can divide it into smaller hypercubes, however you will also need to check for duplicate collisions then, where one foreign object may collide with multiple smaller hypercubes that you inserted.

A new entity is inserted to a grid that fits the entity entirely in one cell, and so that the previous grid's cell size would be too small for the entity (to not insert entities to oversized cells). Due to this, only one copy of the entity is needed over its whole lifetime, and it only occupies one cell in one grid, unlike other structures like a non-hierarchical grid structure, or a QuadTree, in which case you need to be varied of duplicates in leaf nodes and so.

You are free to insert entities that are orders of magnitude bigger than the largest cell in a HSHG, because at the top-most grid, every cell is checked with every other cell, which is a by-product of how the collision works. Additionally, the top-most grid contains 4 cells, not 1.

All entities are stored in one big array that has a fixed size.

Entities may only be altered in `hshg_update()` (`hshg.update`). In all other callbacks, they are read-only. To apply velocity changes that happen during collision or any other changes, you must offload them to some external object that's linked to the HSHG's internal representation of an entity. To do that, insert an entity with its `ref` member set to something that lets you point to the object that the entity owns . This can be done with a simple array holding objects and `ref` being an integer index. You can then access `ref` from within the `hshg.update` callback. Trying to change HSHG's entities while in `hshg.collide` or `hshg.query` will lead to unwanted results, like inaccurate collisions and queries.

### Pitfalls

- This implementation of a HSHG maps the infinite plane to a finite number of cells - entities don't need to always stay on top of the HSHG's area (from `(0, 0)` to whatever `(cells * cell_size, cells * cell_size)` is), they can be somewhere entirely else. However, refrain from doing something like this:

  ```
                       0                1                2
                     2 ----------------------------------- 2
                       |                |                |
                       |                |                |
                       |                |                |
                       |                |                |
                       |   HSHGs area   |   HSHGs area   |
     -1                |                |                |
    1 ---------------------------------------------------- 1
      |                |                |                |
      |   Your arena   |   Your arena   |                |
      |                |                |                |
      |                |                |                |
      |                |   HSHGs area   |   HSHGs area   |
      |                |                |                |
    0 ----------------0,0--------------------------------- 0
      |                |                |                2
      |   Your arena   |   Your arena   |
      |                |                |
      |                |                |
      |                |                |
      |                |                |
   -1 ----------------------------------- -1
     -1                0                1
  ```

  This will result in performance slightly worse than if the HSHG was 4 times smaller in size (one of the cells instead of all 4), because ALL of your arena cells will be mapped to the same exact HSHG cell. If you don't know how this implementation of HSHGs folds the XOY plane to grids, you should just stick to **one** of the four XOY quadrants (as in, maybe make all positions positive instead of having any negatives).

## API

There's no separate headers and source files keeping separate versions of different dimensions. Instead, you need to declare which dimension you want to get definitions for, and whether or not you want the definitions to be stripped of the annoying dimension addon. If you only plan on using one dimension, you can strip the suffix, otherwise you must keep it so that you will be able to use multiple definitions simultaneously.

```c
#define HSHG_D 2
#define HSHG_UNIFORM

#include "hshg.c"
```

You include `c_hierarchical_spatial_hashgrid.h`.

```c
#include "chshg/c_hierarchical_spatial_hashgrid.h"

/* BOTH OF THE BELOW MUST BE POWERS OF 2! */

/* The number of cells on one side of the first,
"tightest" grid (most cells on axis, least cell size).
Square that, and you get the total number of cells. */
const hshg_cell_t cells_on_axis = 64;
/* Side length of one cell in the first, "tightest"
grid. Generally, you will want to make this be as
close to the smallest entity's radius as possible. */
const uint32_t cell_size = 16;

hshg_t* hshg = nhshg::hshg_create(allocator, cells_on_axis, cell_size, max_entities);

class my_entity_event_handler_t : public hshg::entity_event_handler_t {
public:
    virtual void update(entity_t const* e, index_t const* ref, s32 num_entities, entity_functions_t* f)
    {
        // update entities
    }

    virtual void const_update(entity_t const* e, nhshg::index_t const* ref, s32 num_entities, entity_functions_t* f)
    {
        // handle entities
    }

    virtual void collide(entity_t const* e1, nhshg::index_t e1_ref, entity_t const* e2, nhshg::index_t e2_ref) 
    {
        // collision detection
    }

    virtual void query(entity_t const* e, nhshg::index_t e1_ref)
    {
        // query
    }

};

```

Once you're done using the HSHG, you can call `hshg_free(hshg)` to free it. 

All entities have an AABB which is a square. Once collision is detected, it's up to you to provide an algorithm that checks for collision more precisely, if needed. That's why `struct entity_t` only has a radius, no width or height.

Additionally, `collide` is called per every "suspect" pair of entities. They don't even need their AABBs to overlap to be thrown into a broad collision check. It's up to you to provide more definitive ways of checking collision.

Insertion:

```c
const hshg_pos_t x = 0.12;
const hshg_pos_t y = 3.45;
const hshg_pos_t r = 6.78;
const hshg_entity_t ref = 0; /* Not needed for now */
int err = hshg_insert(&hshg, x, y, r, ref);
if(err) {
  /* Out of memory */
}
```

No identifier for the entity is returned from `hshg_insert()`, but one is required for removing entities from the HSHG via `hshg_remove()`. In other words, you may only remove entities from the update callback. However, you may ask how you are supposed to do that, without attaching any metadata to the entity when it's inserted.

That's what the `ref` variable mentioned above achieves - it lets you attach a piece of data (generally an index to a larger array containing lots of data that an entity needs) to the entities you insert. To begin with, you can create an array of data per entity you will want to use:

```c
struct my_entity 
{
    hshg_pos_t vx;
    hshg_pos_t vy;
    int remove_me;
};

#define ABC /* some number */

struct my_entity my_entities[ABC];
```

Now, really, you can do a load of stuff with this to suit it to your needs, like an `hshg_insert()` wrapper that also populates the chosen `my_entities[]` spot, and a method for actually choosing a spot in the array. I'd like to keep it simple, so I will go for a rather dumb solution:

```c++
s32 my_entities_len = 0;

int my_insert(nhshg::entity* ent, struct my_entity* my_ent) {
  my_entities[my_entities_len] = *my_ent;
  const int ret = hshg_insert(hshg, ent->x, ent->y, ent->r, my_entities_len);
  ++my_entities_len;
  return ret;
}
```

Now, from any callback that involves `struct entity_t`, you will also be given the number kept in `ref`, that will then allow you to access that specific spot in the array `entities`. Then, you can implement the process of deleting an entity like so:


```c
void my_entity_handler_t::update(index_t begin, index_t end, entity_t* entities, index_t const* ref, entity_functions_t* f) {
    for(index_t i = start; i < end; ++i)
    {
        my_entity_t* my_ent = my_entities + ref[i];
        if(my_ent->remove_me)
        {
           f->remove(i);
        }
        e[i].x += my_ent->vx;
        e[i].y += my_ent->vy;
        f->move(i);

        // Note: If you change the radius, you must call resize
        // f->resize(i);
    }

}

```

The update callback above can remove entities, and seems to update their position, however the underlying code doesn't actually know the position was updated after you return from the callback. To fix that, call `move(entity index)`, generally after you have finished updating the entity (see above).

You don't need to call `move(entity index)` every single time you change `x` or `y` - you may as well call it once at the end of the update of an entity.

If you are updating the entity's radius too (`entity->r`), you must also call `resize(entity index)`. It works pretty much like `move(entity index)`. If you need to call both, it does not matter in what order you do so.

If you move or update an entity's radius without calling the respective functions at the end of the update callback, collision will not be accurate, query will not return the right entities, stuff will break, and the world is going to end.

If you call `hshg_insert()` from `update`, the newly inserted entity **will not** be updated during the same `update()` function call. 

Note that `remove(entity index)` may **only** be called from `update()`. Same goes for `move()` and `resize()`. These functions do accept an entity index and it is validated by the HSHG to be within the begin/end range.

`hshg_update(hshg)` calls `update` by passing the active range of entities for the user to process. You may not call this function recursively from its callback, nor can you call `hshg_optimize()` and `hshg_collide()`. You are allowed to call `hshg_query()`, however note that you must do so **after** you update positions of entities, which generally will require you to do two `hshg_update()`'s:

```c
void update(struct hshg* hshg, struct hshg_entity* entity) {
  struct my_entity* my_ent = entities + entity->ref;

  if(my_ent->remove_me) {
    hshg_remove(hshg);
    return;
  }

  entity->x += my_ent->vx;
  entity->y += my_ent->vy;

  hshg_move(hshg);
}

const struct hshg_entity* queried_entities[ABC];
hshg_entity_t query_len = 0;

void query(const struct hshg* hshg, const struct hshg_entity* entity) {
  queried_entities[query_len++] = entity;
}

void update_with_query(struct hshg* hshg, struct hshg_entity* entity) {
  struct my_entity* my_ent = entities + entity->ref;

  query_len = 0;
  hshg_query(hshg, minx, miny, maxx, maxy);

  /* do something with the query data stored in queried_entities[] */
}

struct hshg hshg = {0};
hshg.query = query;
assert(!hshg_init(&hshg, 1, 1));

void tick(void) {
  hshg.update = update;
  hshg_update(&hshg);
  hshg.update = update_with_query;
  hshg_update(&hshg);
  /* maybe also collide, etc */
}
```

This sequentiality is required for most projects that want accurate *things*. If you mix updating an entity with viewing an entity's state, all weird sorts of things can happen. Mostly it will be harmless, perhaps minimal visual bugs on the edges of the screen due to incorrect data fetched by `hshg_query()`, but if you don't want that minimal incorrectness (and you probably don't), then separate the concept of modifying `struct hshg_entity` from viewing it. For instance, **NEVER** update an entity in `hshg_collide()`'s callback. Because the next entity the function goes to will see something else than what the previous entity saw.

`hshg_optimize(&hshg)` reallocates all entities and changes the order they are in so that they appear in the most cache friendly way possible. This process insanely speeds up basically all other functions. Moreover, for the duration of the function, the memory usage will nearly double, so if you can't have that, don't use the function.

```c
int err = hshg_optimize(&hshg);
if(err) {
  /* Out of memory */
}
```

While the function helps other functions, it by itself takes a lot of time too. If you want stable performance, your best bet is to call it every single tick, but if you are fine with spikes every now and then, you can call the function every few tens of ticks. This will generally decrease the average time for a tick, but then again, rising from that average will be the call every few tens of ticks, displayed as a big red spike.

`hshg_collide(&hshg)` goes through all entities and detects broad collision between them. It is your responsibility to detect the collision with more detail in the `hshg.collide` callback, if necessary. A sample callback for simple circle collision might look like so:

```c
void collide(const struct hshg* hshg, const struct hshg_entity* a, const struct hshg_entity* b) {
  (void) hshg;
  const float xd = a->x - b->x;
  const float yd = a->y - b->y;
  const float d = xd * xd + yd * yd;
  if(d <= (a->r + b->r) * (a->r + b->r)) { /* if distance less than sum of radiuses */
    const float angle = atan2f(yd, xd);
    const float c = cosf(angle);
    const float s = sinf(angle);
    objs[a->ref].vx += c;
    objs[a->ref].vy += s;
    objs[b->ref].vx -= c;
    objs[b->ref].vy -= s;
  }
}

hshg.collide = collide;
```

From this callback, you may not call `hshg_update()` or `hshg_optimize()`.

`hshg_query(&hshg, min_x, min_y, max_x, max_y)` calls `hshg.query` on every entity that belongs to the rectangular area from `(min_x, min_y)` to `(max_x, max_y)`. It is important that the second and third arguments are smaller or equal to fourth and fifth.

```c
void query(const struct hshg* hshg, const struct hshg_entity* a) {
  draw_a_circle(a->x, a->y, a->r);
}

hshg.query = query;
const hshg_cell_t cells = 128;
const uint32_t cell_size = 128;
assert(!hshg_init(&hshg, cells, cell_size));

/* This is not equal to querying every entity, because
entities don't need to lay within the area of the HSHG
to be properly mapped to cells - they can exist millions
of units away. If you want to limit them to this range,
impose limits on their position in the update callback. */
const uint32_t total_size = cells * cell_size;
hshg_query(&hshg, 0, 0, total_size, total_size);
```

You may not call any of `hshg_update()`, `hshg_optimize()`, or `hshg_collide()` from this callback. You may recursively call `hshg_query()` from its callback.

Summing up all of the above, a normal update tick would look like so:

```c
hshg_update(&hshg);
assert(!hshg_optimize(&hshg)); /* no "no mem" */
hshg_collide(&hshg);
```

Or:

```c
hshg_collide(&hshg);
hshg_update(&hshg);
assert(!hshg_optimize(&hshg));
```

Which is essentially the same as the first one, except not really.

For a complete example, see `c/bench.c` or any of the test suites.

## Optimizations

A few methods were already mentioned above:

- Picking cell size and the number of cells appropriately,
- Calling `hshg_optimize()` only once per a few ticks.

However, there's still room for improvement.

- `hshg_optimize()` causes the internal array of entities of a HSHG to be reordered in a cache-friendly way. That's only part of what entities consist of though - above, `struct my_entity` was an additional part of every entity, with the difference that it existed in a different array. Over time, the order of entities internally will become more and more different from the non-changing array `struct my_entity entities[]`, and so in the update callback, you would be accessing a totally different index internally than in `entities`. That will create cache issues and might slow down the callback even up to 2 times. You can write a function that mitigates this, however at the cost of an additional memory allocation, just like `hshg_optimize()`:

  ```c
  struct my_entity* entities = NULL;
  struct my_entity* entities_new = NULL;
  hshg_entity_t new_idx = 0;

  void update_optimize_entities(struct hshg* hshg, struct hshg_entity* entity) {
    entities_new[new_idx] = entities[a->ref];
    entity->ref = new_idx;
    ++new_idx;
  }

  void optimize_entities(struct hshg* hshg) {
    entities_new = calloc(ABC, sizeof(*entities));
    assert(entities_new);
    new_idx = 0;
    hshg_update_t old = hshg->update;
    hshg->update = update_optimize_entities;
    hshg_update(hshg);
    free(entities);
    entities = entities_new;
    hshg->update = old;
  }

  /* .. and then, in tick .. */
  if(tick_counter % 64 == 0) {
    /* order matters */
    assert(!hshg_optimize(&hshg));
    optimize_entities(&hshg);
  }
  ```

- You might not need to make the HSHG as big as the area you are working with - entities outside of the HSHG's area coverage are still inserted into it, and not on the edge cells like in most QuadTree implementations - they are actually well mapped and spaced out, so basically no performance is lost. Especially in setups where entities are very scattered and not clumped, your performance *might* improve if you decrease the number of cells. On the contrary, increasing the structure's size above of what you need probably won't bring any benefits.

- If your memory is constrained beyond belief, and you certainly won't use a lot of cells and entities, or if you actually have higher requirements than what the defaults are, you might want to opt in changing some constants in the `c/hshg.h` file and recompiling the library (or, if you are simply including the files in your own project, you can redefine these macros before `#include`'ing `hshg.h`). Namely:

  - `hshg_entity_t` - type that fits the number of entities,
  - `hshg_cell_t` - type that fits the number of cells **on one axis**,
  - `hshg_cell_sq_t` - type that fits the total number of cells (might be the same as `hshg_cell_t`),
  - `hshg_pos_t` - type that `x`, `y`, and `r` are using (`float` by default).

  For the first 3, there's no need to make them signed. As for `hshg_pos_t`, only floating-point types are allowed.

  Say you won't use more than 10,000 entities, and you want to have 64x64 cells, 32x32 cell size each. In that case, you may set these constants to the following:

  ```c
  #define hshg_entity_t  uint16_t
  #define hshg_cell_t    uint8_t
  #define hshg_cell_sq_t uint16_t
  ```

  That will save up a few bytes, not only in `struct hshg_entity`, but also in `struct hshg`, and note that each cell is of type `hshg_entity_t`, so the smaller that is, the less memory cells will use. With the above settings, your simulation would use roughly 250KB (not counting in memory that might be allocated by `hshg_optimize()` and any custom data you might want to keep alongside entities in a separate array).
