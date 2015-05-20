/* PANDABEGINCOMMENT
 * 
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu        
 *  Tom Boning             tboning@mit.edu
 * 
 * This work is licensed under the terms of the GNU GPL, version 2. 
 * See the COPYING file in the top-level directory.    
 * 
 PANDAENDCOMMENT */

/*
  This plugin is based on asidstory, and is designed to fetch information
  about the process tree structure and display it.

 */


// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <algorithm>
#include <map>
#include <set>
#include <cstdint>

extern "C" {

#include "panda_plugin.h"
#include "panda_common.h"
#include "pandalog.h"

#include "rr_log.h"
#include "rr_log_all.h"  
#include "../osi/osi_types.h"
#include "../osi/osi_ext.h"
#include "panda_plugin_plugin.h"
    
    bool init_plugin(void *);
    void uninit_plugin(void *);

}

#define SAMPLE_CUTOFF 10    
#define SAMPLE_RATE 1
#define MILLION 1000000

uint64_t a_counter = 0;
uint64_t b_counter = 0;
bool first_walk = false;

typedef std::string Name;
typedef uint32_t Pid;
typedef uint32_t Ppid;
typedef uint64_t Asid;
typedef uint32_t Cell;
typedef uint64_t Count;
typedef uint64_t Instr;

//std::map < Name, std::map < Pid, std::map < Cell, Count > > > namepid_cells;
std::map < Name, std::map < Pid, Asid > > namepid_to_asids;
std::map < Name, std::map < Pid, std::pair <Name, Ppid> > > namepid_to_pnameppid;

//std::map < Pid, Name > pid_to_name;
//std::map < Pid, Ppid > pid_to_parent;
//std::map < Pid, Asid > pid_to_asid;

// Yes this is expensive. Thankfully we only need to do it once per proc we find.
Name get_name_from_pid(Pid pid) {
  for (auto &kvp1 : namepid_to_asids) {
    Name name = kvp1.first;
    for (auto &kvp2 : kvp1.second) {
      Pid p = kvp2.first;
      if (pid == p) {
	return name;
      }
    }
  }
  return "";
}

void spit_procstory() {
    
  FILE *fp = fopen("procstory", "w");
  // write out concordance namepid to asid 
  for ( auto &kvp1 : namepid_to_asids ) {
    Name name = kvp1.first;
    for (auto &kvp2 : kvp1.second) {
      Pid pid = kvp2.first;
      Asid asid = kvp2.second;

      char nps[256];
      snprintf(nps, 256, "%d-%s", pid, (const char *) name.c_str());

      fprintf (fp, "%20s : ", nps);
      fprintf (fp, "(asid: 0x%08x, ", (unsigned int) asid);

      char pps[256];
      std::pair <Name, Ppid> p = namepid_to_pnameppid[name][pid];
      Ppid ppid = p.second;
      Name pname = p.first;
      snprintf(pps, 256, "%d-%s", ppid, (const char *) pname.c_str());

      fprintf (fp, "parent: %20s)", pps);
      fprintf (fp, "\n");
    }
  }
  fclose(fp);
}


char *last_name = 0;
target_ulong last_pid = 0;
target_ulong last_asid = 0;

Panda__Process *create_panda_process (uint32_t pid, char *name) {
  Panda__Process *p = (Panda__Process *) malloc(sizeof(Panda__Process));
  *p = PANDA__PROCESS__INIT;
  p->pid = pid;
  p->name = name;
  return p;
}



int procstory_after_block_exec(CPUState *env, TranslationBlock *tb, TranslationBlock *tb2)  {

  if ((a_counter % 1000000) == 0) {
    spit_procstory();
  }
  a_counter++;

  if (!first_walk) { 
    OsiProcs *p = get_processes(env);
    if (p == NULL) {
      free (p);
      return 0;
    }
  
    for (uint32_t i=0; i < p->num; i++) {
      OsiProc proc = p->proc[i];
      namepid_to_asids[proc.name][proc.pid] = proc.asid;
      //Pid pid = proc.pid;
      //pid_to_name[pid] = proc.name;
      //pid_to_parent[pid] = proc.ppid;
      //pid_to_asid[pid] = proc.asid;
    }
    for (uint32_t i=0; i < p->num; i++) {
      OsiProc proc = p->proc[i];
      Name pname = get_name_from_pid(proc.ppid);
      namepid_to_pnameppid[proc.name][proc.pid] = std::pair<Name, Ppid>(pname, proc.ppid);
      if (pandalog) {
        Panda__LogEntry ple = PANDA__LOG_ENTRY__INIT;
        /* // asidstory style
	ple.has_asid = 1;
        ple.asid = proc.asid;
        ple.has_process_id = 1;
        ple.process_id = proc.pid;
        ple.process_name = proc.name;*/
	// win7proc style (fake NTCreateUserProcess)
	Panda__Process *cur_p = create_panda_process(proc.ppid, (char *)pname.c_str());
	Panda__Process *new_p = create_panda_process(proc.pid, proc.name);
	Panda__NtCreateUserProcess *ntcup = 
	  (Panda__NtCreateUserProcess *) malloc (sizeof(Panda__NtCreateUserProcess));
	*ntcup = PANDA__NT_CREATE_USER_PROCESS__INIT;
	ntcup->new_long_name = proc.name;
	ntcup->cur_p = cur_p;
	ntcup->new_p = new_p;
	ple.nt_create_user_process = ntcup;
        pandalog_write_entry(&ple);
      }
    }
  
    free (p);
    first_walk = true;
    return 0;
  } else {
    return 0;
  }
  if ((a_counter % SAMPLE_RATE) != 0) {
    return 0;
  }

  OsiProc *p = get_current_process(env);
  //if (pid_ok(p->pid)) {
  if (true) {
    //update stored information
    namepid_to_asids[p->name][p->pid] = p->asid;
    Name pname = get_name_from_pid(p->ppid);
    namepid_to_pnameppid[p->name][p->pid] = std::pair<Name, Ppid>(pname, p->ppid);
    //Output to pandalog
    //TODO once we figure out what info to output
  }
  free(p);

  return 0;
}

bool init_plugin(void *self) {    

  printf ("Initializing plugin procstory\n");
  
  panda_require("osi");
   
  // this sets up OS introspection API
  bool x = init_osi_api();  
  assert (x==true);
  
  panda_cb pcb;    
  pcb.after_block_exec = procstory_after_block_exec;
  panda_register_callback(self, PANDA_CB_AFTER_BLOCK_EXEC, pcb);
  
  if (pandalog){
    printf("Pandalog = true\n");
  } else {
    printf("Pandalog = false\n");
  }

  // pcb.after_block_exec = procstory_after_block_exec;
  // panda_register_callback(self, PANDA_CB_AFTER_BLOCK_EXEC, pcb);
  
  // panda_arg_list *args = panda_get_args("procstory");
  // vol_instr_count = panda_parse_uint64(args, "vol_instr_count", 0);
  // vol_cmds = panda_parse_string(args, "vol_cmds", "xxx");
  
  return true;
}



void uninit_plugin(void *self) {
  spit_procstory();
}

