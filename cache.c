/*
 * cache.c
 */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE; 
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;

/* cache model data structures */
static Pcache icache;
static Pcache dcache;
static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;

/************************************************************/
void set_cache_param(param, value)
  int param;
  int value;
{

  switch (param) {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }

}
/************************************************************/

/************************************************************/
void init_cache()
{
  // Init stats
  // I-CACHE stats
  cache_stat_inst.accesses = 0;
  cache_stat_inst.misses = 0;
  cache_stat_inst.replacements = 0;
  cache_stat_inst.demand_fetches = 0;
  cache_stat_inst.copies_back = 0;
  // D-CACHE stats
  cache_stat_data.accesses = 0;
  cache_stat_data.misses = 0;
  cache_stat_data.replacements = 0;
  cache_stat_data.demand_fetches = 0;
  cache_stat_data.copies_back = 0;

  // Init cache structures
  if (cache_split == FALSE) {
    // Unified cache
    icache = &c1;
    dcache = &c1; 
    // Init U-CACHE
    c1.size = cache_usize;
    c1.associativity = cache_assoc;
    c1.n_sets = cache_usize / (cache_block_size * cache_assoc);
    c1.index_mask = ((1 << (int)LOG2(c1.n_sets)) - 1) << LOG2(cache_block_size);
    c1.index_mask_offset = LOG2(cache_block_size);
    c1.LRU_head = (Pcache_line *)malloc(c1.n_sets * sizeof(Pcache_line));
    c1.LRU_tail = (Pcache_line *)malloc(c1.n_sets * sizeof(Pcache_line));
    c1.set_contents = (int *)malloc(c1.n_sets * sizeof(int));
    c1.contents = 0;
    // Initialize each set
    for (int i = 0; i < c1.n_sets; i++)
    {
      c1.LRU_head[i] = (Pcache_line)NULL;
      c1.LRU_tail[i] = (Pcache_line)NULL;
      c1.set_contents[i] = 0;
    }
    return;
  } else {
    // Split cache
    icache = &c1;
    dcache = &c2;
    // Init I-CACHE
    c1.size = cache_isize;
    c1.associativity = cache_assoc;
    c1.n_sets = cache_isize / (cache_block_size * cache_assoc);
    c1.index_mask = ((1 << (int)LOG2(c1.n_sets)) - 1) << LOG2(cache_block_size);
    c1.index_mask_offset = LOG2(cache_block_size);
    c1.LRU_head = (Pcache_line *)malloc(c1.n_sets * sizeof(Pcache_line));
    c1.LRU_tail = (Pcache_line *)malloc(c1.n_sets * sizeof(Pcache_line));
    c1.set_contents = (int *)malloc(c1.n_sets * sizeof(int));
    c1.contents = 0;
    // Initialize each set
    for (int i = 0; i < c1.n_sets; i++)
    {
      c1.LRU_head[i] = (Pcache_line)NULL;
      c1.LRU_tail[i] = (Pcache_line)NULL;
      c1.set_contents[i] = 0;
    }
    // Init D-CACHE
    c2.size = cache_dsize;
    c2.associativity = cache_assoc;
    c2.n_sets = cache_dsize / (cache_block_size * cache_assoc);
    c2.index_mask = ((1 << (int)LOG2(c2.n_sets)) - 1) << LOG2(cache_block_size);
    c2.index_mask_offset = LOG2(cache_block_size);
    c2.LRU_head = (Pcache_line *)malloc(c2.n_sets * sizeof(Pcache_line));
    c2.LRU_tail = (Pcache_line *)malloc(c2.n_sets * sizeof(Pcache_line));
    c2.set_contents = (int *)malloc(c2.n_sets * sizeof(int));
    c2.contents = 0;
    // Initialize each set
    for (int i = 0; i < c2.n_sets; i++)
    {
      c2.LRU_head[i] = (Pcache_line)NULL;
      c2.LRU_tail[i] = (Pcache_line)NULL;
      c2.set_contents[i] = 0;
    }
  }
}
/************************************************************/

/************************************************************/
void perform_access(addr, access_type)
  unsigned addr, access_type;
{
  // pointers to the cache and stats to use
  Pcache pc;
  Pcache_stat ps;

  if (access_type == TRACE_INST_LOAD) {
    pc = icache;
    ps = &cache_stat_inst;
  } else {
    pc = dcache;
    ps = &cache_stat_data;
  }
  if (!pc) return;

  ps->accesses++;

  // start of cache access logic
  // 1. calculate set and tag
  unsigned set, tag;
  int index_bits;
  set = (addr & pc->index_mask) >> pc->index_mask_offset;
  index_bits = LOG2(pc->n_sets);
  tag = addr >> (pc->index_mask_offset + index_bits);

  // 2. search in the set's LRU list
  Pcache_line cacheEntry;
  cacheEntry = pc->LRU_head[set];
  while (cacheEntry && cacheEntry->tag != tag) cacheEntry = cacheEntry->LRU_next;

  // 3. check hit or miss
  if (cacheEntry) {
    // HIT
    if (access_type == TRACE_DATA_STORE) {
      if (cache_writeback) {
        // write cache line only
        cacheEntry->dirty = 1; 
      } else {
        // write cache line and memory (1 word)
        ps->copies_back += 1;
      }
    }
    // move line to head of LRU list
    if (pc->LRU_head[set] != cacheEntry) {
      delete(&pc->LRU_head[set], &pc->LRU_tail[set], cacheEntry);
      insert(&pc->LRU_head[set], &pc->LRU_tail[set], cacheEntry);
    }
    return;
  }

  // MISS
  ps->misses++;
  // Store miss not write allocate
  if (access_type == TRACE_DATA_STORE && !cache_writealloc) {
    // write to memory directly (1 word)
    ps->copies_back += 1;
    return;
  }

  // other misses
  // fetch block from memory
  ps->demand_fetches += words_per_block;
  // evict if set is full
  if (pc->set_contents[set] >= pc->associativity) {
    Pcache_line evictedCacheLine = pc->LRU_tail[set]; // lru at tail
    // write back if dirty
    if (evictedCacheLine && evictedCacheLine->dirty && cache_writeback) {
      // write back entire block to memory
      ps->copies_back += words_per_block; 
    }
    if (evictedCacheLine) {
      // remove from LRU list
      delete(&pc->LRU_head[set], &pc->LRU_tail[set], evictedCacheLine);
      free(evictedCacheLine);
      pc->set_contents[set]--;
      ps->replacements++;
    }
  }

  // Insert new line at head
  Pcache_line newline = (Pcache_line)malloc(sizeof(cache_line));
  newline->tag = tag;
  newline->dirty = 0;
  newline->LRU_next = NULL;
  newline->LRU_prev = NULL;

  if (access_type == TRACE_DATA_STORE) {
    if (cache_writeback) {
      newline->dirty = 1; 
    } else {
      ps->copies_back += 1; 
    }
  }

  insert(&pc->LRU_head[set], &pc->LRU_tail[set], newline);
  pc->set_contents[set]++;
  pc->contents++;
}
/************************************************************/

/************************************************************/
void flush()
{

  /* flush the cache */

}
/************************************************************/

/************************************************************/
void delete(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;

  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

/************************************************************/
void dump_settings()
{
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split) {
    printf("  Split I- D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  } else {
    printf("  Unified I- D-cache\n");
    printf("  Size: \t%d\n", cache_usize);
  }
  printf("  Associativity: \t%d\n", cache_assoc);
  printf("  Block size: \t%d\n", cache_block_size);
  printf("  Write policy: \t%s\n", 
	 cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("  Allocation policy: \t%s\n",
	 cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}
/************************************************************/

/************************************************************/
void print_stats()
{
  printf("\n*** CACHE STATISTICS ***\n");

  printf(" INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  if (!cache_stat_inst.accesses)
    printf("  miss rate: 0 (0)\n"); 
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n", 
	 (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
	 1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf(" DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  if (!cache_stat_data.accesses)
    printf("  miss rate: 0 (0)\n"); 
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n", 
	 (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
	 1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf(" TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches + 
	 cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/
