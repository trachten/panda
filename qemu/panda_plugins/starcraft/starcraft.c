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

#include "config.h"
#include "qemu-common.h"

#include "panda_plugin.h"
#include "rr_log.h"

bool init_plugin(void *);
void uninit_plugin(void *);

int before_block_exec(CPUState *env, TranslationBlock *tb);

int before_block_exec(CPUState *env, TranslationBlock *tb) {
    return 0;
}

bool translate_callback(CPUState *env, target_ulong pc) {
    return env->cr[3] == 0x06cba000 && (pc == 0x0044C130 || pc == 0x0044C836);
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
        printf("[%" PRIu64 "] decrypt_key: \n", rr_get_guest_instr_count());
    }
    return 1;
}   

bool init_plugin(void *self) {
    panda_cb pcb = { .insn_translate = translate_callback};
    panda_register_callback(self, PANDA_CB_INSN_TRANSLATE, pcb);
    pcb.insn_exec = exec_callback;
    panda_register_callback(self, PANDA_CB_INSN_EXEC, pcb);

    return true;
}

void uninit_plugin(void *self) { }
