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
#include "panda_plugin_plugin.h"

#include "osi_types.h"
#include "osi_int_fns.h"
#include "os_intro.h"

#include "../win7x86intro/win7x86intro.h"

//#include "osi.h"

bool init_plugin(void *);
void uninit_plugin(void *);

PPP_PROT_REG_CB(on_get_processes)
PPP_PROT_REG_CB(on_get_current_process)
PPP_PROT_REG_CB(on_get_modules)
PPP_PROT_REG_CB(on_get_libraries)
PPP_PROT_REG_CB(on_free_osiproc)
PPP_PROT_REG_CB(on_free_osiprocs)
PPP_PROT_REG_CB(on_free_osimodules)
PPP_PROT_REG_CB(on_process_start);
PPP_PROT_REG_CB(on_process_end);

PPP_CB_BOILERPLATE(on_get_processes)
PPP_CB_BOILERPLATE(on_get_current_process)
PPP_CB_BOILERPLATE(on_get_modules)
PPP_CB_BOILERPLATE(on_get_libraries)
PPP_CB_BOILERPLATE(on_free_osiproc)
PPP_CB_BOILERPLATE(on_free_osiprocs)
PPP_CB_BOILERPLATE(on_free_osimodules)
PPP_CB_BOILERPLATE(on_process_start)
PPP_CB_BOILERPLATE(on_process_end)


// The copious use of pointers to pointers in this file is due to
// the fact that PPP doesn't support return values (since it assumes
// that you will be running multiple callbacks at one site)

OsiProcs *get_processes(CPUState *env) {
    OsiProcs *p = NULL;
    PPP_RUN_CB(on_get_processes, env, &p);
    return p;
}

OsiProc *get_current_process(CPUState *env) {
    OsiProc *p = NULL;
    PPP_RUN_CB(on_get_current_process, env, &p);
    return p;
}

OsiModules *get_modules(CPUState *env) {
    OsiModules *m = NULL;
    PPP_RUN_CB(on_get_modules, env, &m);
    return m;
}

OsiModules *get_libraries(CPUState *env, OsiProc *p) {
    OsiModules *m = NULL;
    PPP_RUN_CB(on_get_libraries, env, p, &m);
    return m;
}

void free_osiproc(OsiProc *p) {
    PPP_RUN_CB(on_free_osiproc, p);
}

void free_osiprocs(OsiProcs *ps) {
    PPP_RUN_CB(on_free_osiprocs, ps);
}

void free_osimodules(OsiModules *ms) {
    PPP_RUN_CB(on_free_osimodules, ms);
}


// NB: this isn't part of the api. 
// this gets called when a process gets created
void osi_process_start(CPUState *env, uint64_t pc, OsiProc *proc) {
    PPP_RUN_CB(on_process_start, env, pc, proc);
}

// NB: also not part of the api.  
// gets called when a process terminates
void osi_process_end(CPUState *env, uint64_t pc, OsiProc *proc) {
    PPP_RUN_CB(on_process_end, env, pc, proc);
}

bool init_plugin(void *self) {
    // we want these to run when process is created / terminated

    PPP_REG_CB("win7x86intro", on_win7x86_process_start, osi_process_start); 
    PPP_REG_CB("win7x86intro", on_win7x86_process_end, osi_process_end); 
    // TODO:  add same for other operating systems (linux x86, linux arm)

    return true;
}

void uninit_plugin(void *self) { }
