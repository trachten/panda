extern "C" {
#include <sys/mman.h>
}

#include <map>
#include <vector>

#include "label_set.h"

class LabelSetAlloc {
private:
    uint8_t *next = NULL;
    std::vector<std::pair<uint8_t *, size_t>> blocks;
    size_t next_block_size = 1 << 15;

    void alloc_block() {
        //printf("taint2: allocating block of size %lu\n", next_block_size);
        next = (uint8_t *)mmap(NULL, next_block_size, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(next);
        blocks.push_back(std::make_pair(next, next_block_size));
        next_block_size <<= 1;
    }

public:
    LabelSetAlloc() {
        alloc_block();
    }

    LabelSetP alloc() {
        assert(blocks.size() > 0);
        std::pair<uint8_t *, size_t>& block = blocks.back();
        if (next + sizeof(struct LabelSet) > block.first + block.second) {
            alloc_block();
            assert(next != NULL);
        }

        LabelSetP result = new(next) struct LabelSet;
        result->child1 = nullptr;
        result->label = ~0U;
        result->count = 0;
        next += sizeof(struct LabelSet);
        return result;
    }

    ~LabelSetAlloc() {
        for (auto&& block : blocks) {
            munmap(block.first, block.second);
        }
    }
} LSA;

std::map<std::pair<LabelSetP, LabelSetP>, LabelSetP> memoized_unions;


std::set<uint32_t> label_set_render_set_2(LabelSetP ls) {
    return label_set_iter<std::set<uint32_t>, set_insert>(ls);
}


// map from set to a unique int
std::map < std::set<uint32_t>, uint32_t > set_to_int;
// map from unique int to set
std::map < uint32_t, std::set<uint32_t> > int_to_set;


void spit_set( std::set<uint32_t> s) {
    for ( auto el : s ) {
        printf ("%d ", el);
    }
}


// keep track of mapping from label set pointer 
// to unique int 
std::map<LabelSetP, uint32_t> lsp_to_int;
std::map<uint32_t, LabelSetP> int_to_lsp;


// keep only one copy of a set
// referenced by an integer


uint64_t num_hits = 0;
uint64_t num_misses = 0;

uint32_t register_set( LabelSetP lsp, std::set<uint32_t> s ) {
    uint32_t i;

    if (set_to_int.count(s) == 0) {
        printf ("register_set 0x%" PRIx64" : ", (uint64_t) lsp);
        spit_set(s);
        printf ("\n");
        i = set_to_int.size();
        set_to_int[s] = i;
        int_to_set[i] = s;
        printf ("i=%d\n", i);

        printf ("%d in set_to_int \n", (int) set_to_int.size());
        printf ("%d in int_to_set \n", (int) int_to_set.size());
        printf ("%d in lsp_to_int \n", (int) lsp_to_int.size());
        printf ("%d in int_to_lsp \n", (int) int_to_lsp.size());

        printf ("%.5f hit rate\n", ((float) num_hits) / (num_hits + num_misses));

    }
    else {
        i = set_to_int[s];
    }

    // also keep track of mapping from label-set-pointer to set_index and back.
    lsp_to_int[lsp] = i;
    int_to_lsp[i] = lsp;

        

    return i;
}

bool check_set( std::set<uint32_t> s) {
    if (set_to_int.count(s) != 0) {
        return true;
    }
    return false;
}

std::set<uint32_t> label_set_render_set(LabelSetP ls) {
    // NB: rendering *must* have happened already and been cached
    assert (lsp_to_int.count(ls) != 0);
    uint32_t i = lsp_to_int[ls];
    assert (int_to_set.count(i) != 0);
    return int_to_set[i];
}


LabelSetP label_set_union(LabelSetP ls1, LabelSetP ls2) {
    if (ls1 == ls2) {
        return ls1;
    }
    else if (ls1 && ls2) {
        LabelSetP min = std::min(ls1, ls2);
        LabelSetP max = std::max(ls1, ls2);
        
        //qemu_log_mask(CPU_LOG_TAINT_OPS, "  MEMO: %lu, %lu\n", (uint64_t)min, (uint64_t)max);

        std::pair<LabelSetP, LabelSetP> minmax(min, max);

         auto it = memoized_unions.find(minmax);
        if (it != memoized_unions.end()) {
            //qemu_log_mask(CPU_LOG_TAINT_OPS, "  FOUND\n");
            num_hits ++ ;
            return it->second;
        }
        num_misses ++ ;

        if (((num_hits + num_misses) % 10000) == 0) {
            printf ("h=%" PRIu64 " m=%" PRIu64 " \n", num_hits, num_misses);
        }


        //qemu_log_mask(CPU_LOG_TAINT_OPS, "  NOT FOUND\n");


        printf ("min=0x%lx max=0x%lx is not found\n", (uint64_t) min, (uint64_t) max);

        // we have a new minmax (pair of two sets we are supposed to union)
        // get rendered versions of both min and max, then compute
        // rendered union
        
        // integers min_i, max_i each reference a rendered set.  
        // NB: if two sets contains same elements, then they are indexed by the same int
        uint32_t min_i, max_i;
        
        if (lsp_to_int.count(min) == 0) {
            // min is a new set
            auto mins = label_set_render_set_2(min);
            min_i = register_set(min,mins);
        }
        else {
            // old set -- no need to recompute
            min_i = lsp_to_int[min];
        }
        
        if (lsp_to_int.count(max) == 0) {
            auto maxs = label_set_render_set_2(max);
            max_i = register_set(max,maxs);
        }
        else {
            max_i = lsp_to_int[max];
        }


        // "render" union into result_set
        auto mins = int_to_set[min_i];
        auto maxs = int_to_set[max_i];
        std::set<uint32_t> result_set;
        for ( auto el : mins ) {
            result_set.insert(el);
        }
        for ( auto el : maxs ) {
            result_set.insert(el);
        }
        
        // is result actually a new set?
        if (set_to_int.count(result_set) != 0) {
            // no -- so just return a LabelSetP that already points to that rendered set
            uint32_t result_i =  set_to_int[result_set];
            LabelSetP result = int_to_lsp[result_i];
            memoized_unions.insert(std::make_pair(minmax, result));
            return result;
        }
        // okay, yes, result really is a new set.  we need to allocate a new label set 
        // and record the pair, as well as register the rendered set.


        LabelSetP result = LSA.alloc();
        //labelset_count++;

        result->child1 = min;
        result->child2 = max;
        // this is taint compute #?  ok that will be bollocksed.  
        result->count = min->count + max->count;
        if (result->count < min->count) result->count = ~0UL; // handle overflows

        memoized_unions.insert(std::make_pair(minmax, result));
        //qemu_log_mask(CPU_LOG_TAINT_OPS, "  INSERTED\n");

        register_set(result, result_set);
        //        printf ("resulting union is new %d %" PRIx64"\n", x, (uint64_t) result);


        return result;
    } else if (ls1) {
        return ls1;
    } else if (ls2) {
        return ls2;
    } else return nullptr;
}

LabelSetP label_set_singleton(uint32_t label) {
    LabelSetP result = LSA.alloc();
    //labelset_count++;

    result->child1 = nullptr;
    result->label = label;
    result->count = 1;

    std::set<uint32_t> result_set =  label_set_render_set_2(result);
    register_set(result, result_set);

    return result;
}

uint64_t label_set_render_uint(LabelSetP ls) {
    constexpr uint64_t zero = 0UL;
    return label_set_iter<uint64_t, bitset_insert, zero>(ls);
}
