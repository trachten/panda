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

extern "C" {

#include "config.h"
#include "qemu-common.h"

#include "rr_log.h"

#include "panda/panda_addr.h"

#include "panda_plugin.h"
#include "panda_plugin_plugin.h"

}

#include "../taint2/taint2.h"
#include "../taint2/taint2_ext.h"

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {

bool init_plugin(void *);
void uninit_plugin(void *);

}

FILE *tcnlog;
uint32_t max_tcn = 0;

void taint_change(Addr a) {
    uint32_t tcn = taint2_query_tcn(a);
    if (tcn > max_tcn) {
        max_tcn = tcn;
        fprintf(tcnlog, "%" PRIu64 " %u\n", rr_get_guest_instr_count(), max_tcn);
    }
}

bool init_plugin(void *self) {
    panda_require("taint2");
    assert(init_taint2_api());
    PPP_REG_CB("taint2", on_taint_change, taint_change);
    taint2_track_taint_state();

    panda_arg_list *args = panda_get_args("tcn");
    const char *logname = panda_parse_string(args, "output", "tcn.dat");
    tcnlog = fopen(logname, "w");
    if (!tcnlog) {
        perror("fopen");
        return false;
    }
    return true;
}

void uninit_plugin(void *self) {
    fclose(tcnlog);
}
