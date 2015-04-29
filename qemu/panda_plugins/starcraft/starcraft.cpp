/* PANDABEGINCOMMENT
 * 
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu
 * 
 * This work is licensed under the terms of the GNU GPL, version 2. 
 * See the COPYING file in the top-level directory. 
 * 
PANDAENDCOMMENT */
// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

#include "../taint2/taint2.h"

extern "C" {
#include "config.h"
#include "qemu-common.h"

#include "panda/panda_addr.h"

#include "panda_plugin.h"
#include "panda_plugin_plugin.h"
#include "panda_common.h"

#include "../taint2/taint2_ext.h"

#include "rr_log.h"

bool init_plugin(void *);
void uninit_plugin(void *);

bool before_block_exec(CPUState *env, TranslationBlock *tb);

}

bool before_block_exec(CPUState *env, TranslationBlock *tb) {
    printf("pc=0x" TARGET_FMT_lx "\n", tb->pc);
    if (tb->pc == 0x0047D490) {
        taint2_enable_taint();           
        target_ulong the_buf = EAX;
        target_ulong the_len = 26;
        // taint what EAX points to here
        printf ("\n****************************************************************************\n");
        printf ("applying taint labels to search string of length %d  @ p=0x" TARGET_FMT_lx "\n", the_len, the_buf);
        printf ("******************************************************************************\n");
        // label that buffer 
        for (unsigned i=0; i<the_len; i++) {
            target_ulong va = the_buf + i;
            target_phys_addr_t pa = cpu_get_phys_addr(cpu_single_env, va);
            if (pa != (target_phys_addr_t) -1) {
                taint2_label_ram(pa, i);
            }
        }
        return true;
    }
    return false;
}

bool translate_callback(CPUState *env, target_ulong pc) {
    return env->cr[3] == 0x06cba000 && (pc == 0x0044C130 || pc == 0x0044C836 || pc == 0x0047D450 || pc == 0x0044C120);
}   
    
int exec_callback(CPUState *env, target_ulong pc) {
    if (pc == 0x0044C130) {
        printf("[%" PRIu64 "] Inside test_key: \n", rr_get_guest_instr_count());

        target_ulong x = 0;
        panda_virtual_memory_rw(env, EAX, (uint8_t *)&x, 4, 0);
         
        printf("  Expected=" TARGET_FMT_lx " calculated=" 
            TARGET_FMT_lx "\n", x, ECX);
    }
    else if (pc == 0x0044C836) {
        printf("[%" PRIu64 "] convert_to_base_5_scramble \n", rr_get_guest_instr_count());
        printf("  key=" TARGET_FMT_lx "\n", EDI);
    }
    else if (pc == 0x0047D450) {
        printf("[%" PRIu64 "] first_checksum \n", rr_get_guest_instr_count());
    }
    else if (pc == 0x0044C130) {
        printf("[%" PRIu64 "] find_val \n", rr_get_guest_instr_count());
    }
    return 1;
}   

bool init_plugin(void *self) {
    panda_cb pcb = { .insn_translate = translate_callback};
    panda_register_callback(self, PANDA_CB_INSN_TRANSLATE, pcb);
    pcb.insn_exec = exec_callback;
    panda_register_callback(self, PANDA_CB_INSN_EXEC, pcb);
    pcb.before_block_exec_invalidate_opt = before_block_exec;
    panda_register_callback(self, PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT, pcb);

    panda_require("taint2");

    assert(init_taint2_api());


    return true;
}

void uninit_plugin(void *self) { }
