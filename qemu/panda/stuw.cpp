
// cd panda/qemu
// g++ -g -o stuw stuw.cpp pandalog.c pandalog_print.c pandalog.pb-c.c  -L/usr/local/lib -lprotobuf-c -I .. -lz -D PANDALOG_READER  -std=c++11

#define __STDC_FORMAT_MACROS

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
}

#include <inttypes.h>
#include <tuple>
#include <vector>
#include <map>
#include <set>

#include "pandalog.h"
#include "pandalog_print.h"

typedef struct process_struct {
    uint32_t pid;
    char *name;
} Process;

void print_process(Process &p) {
    printf ("proc(%d,%s)", p.pid, p.name);
}

struct NamePid {
    std::string name;
    uint32_t pid;

    NamePid(std::string name, uint32_t pid) :
	name(name), pid(pid) {}

    bool operator<(const NamePid &rhs) const {
	return name < rhs.name || (name == rhs.name && pid < rhs.pid);
    }
};

void print_namepid(NamePid proc){
    printf("proc(%d,%s)",proc.pid, proc.name.c_str());
}

std::set<NamePid>procs;



// instruction count
typedef uint64_t Instr;
// program counter
typedef uint64_t Pc;

enum Direction {In=0, Out=1, Both=2};
const char * dirname[] = {"in", "out", "both"};

bool processes_equal(Process &p1, Process &p2) {
    if ((p1.pid != p2.pid) && (0 != strcmp(p1.name, p2.name))) {
	return false;
    }
    return true;
}

enum StoreType {File=0, RegKeyVal=1, LocalPort=2, Section=3, VM=4};
const char * storename[] = {"file", "registry", "ALPC", "section", "virtualmemory"};

typedef struct file_data_struct {
    char *filename;
} FileData;

typedef struct reg_key_val_data_struct {
    char *keyname;
    char *valname;
    uint32_t index;
} RegKeyValData;

typedef struct local_port_data_struct {
    uint32_t id; //just for our purposes
    Process client;
    Process server;
} LocalPortData;

typedef struct section_data_struct {
    uint32_t id;
    char *name;
    char *file_name;
} SectionData;

typedef struct store_struct {
    StoreType type;
    union {
        FileData fd;
        RegKeyValData kvd;
        LocalPortData lpd;
        SectionData sd;
    } val;
} Store;

typedef struct full_flow_struct {
    Instr instr;
    Pc pc;
    Process sender;
    Process target;
    Direction dir;
    Store store;
} FullFlow;

std::vector<FullFlow> fullflows;

bool stores_equal(Store &s1, Store &s2) {
    if (s1.type != s2.type) return false;
    switch (s1.type) {
    case File:
        if (0 == strcmp(s1.val.fd.filename, s2.val.fd.filename))
            return true;
        else 
            return false;
        break;
    case RegKeyVal:
        if ((0 == strcmp(s1.val.kvd.keyname, s2.val.kvd.keyname))
            && (0 == strcmp(s1.val.kvd.valname, s2.val.kvd.valname)))
            return true;
        else
            return false;
        break;
    case LocalPort:
        break;
    case Section:
        if (s1.val.sd.id == s2.val.sd.id) {
            return true;
        } else {
            return false;
        }
        break;
    default:
        break;
    }
    return false;
}

void print_store(Store &s) {
    switch (s.type) {
    case File:
        printf ("file(%s)", s.val.fd.filename);
        break;
    case RegKeyVal:
        printf ("reg(key=%s,val=%s,i=%d)", s.val.kvd.keyname, s.val.kvd.valname, s.val.kvd.index);
        break;
    case Section:
	printf("section(id = %x name = ", s.val.sd.id);
	if (s.val.sd.name != NULL){
	    printf("%s", s.val.sd.name);
	}
	printf(",file_name = ");
	// segfaulting, idk why
	if (s.val.sd.file_name != NULL){
	    //    printf("%s", s.val.sd.file_name);//, s.val.sd.name, s.val.sd.file_name);
	}
	printf(")");
        break;
    case LocalPort:
	printf("ALPC(client = ");
	print_process(s.val.lpd.client);
	printf(", server = ");
	print_process(s.val.lpd.server);
	printf(")");
        break;
    default:
        break;
    }
}

typedef struct flow_struct {
    Instr instr;
    Pc pc;
    Direction dir;
    Process process;
    Store store;
} Flow;
    
void print_flow(Flow &flow) {
    printf ("instr=%" PRIu64 " pc=0x%" PRIx64 " ", flow.instr, flow.pc);
    if (flow.dir == In) {
	print_store(flow.store);
	printf (" -> ");
	print_process(flow.process);
    }
    if (flow.dir == Out) {
        print_process(flow.process);
        printf (" -> ");
        print_store(flow.store);
    }
}

// flows leaving a process
std::vector < Flow > ins;

// flows entering a process
std::vector < Flow > outs;


void print_full_flow(FullFlow flow) {
    printf ("instr=%" PRIu64 " pc=0x%" PRIx64 " ", flow.instr, flow.pc);
    print_process(flow.sender);
    printf(" <-> ");
    printf("\n");
    print_store(flow.store);
    printf("\n");
    printf(" <-> ");
    print_process(flow.target);
}

std::map<NamePid, std::map<NamePid, int>> proc_comm_count;
std::map<NamePid, std::map<NamePid, std::map<StoreType, int>>> proc_comm_type_count;

void flow_match() {
    for ( auto &outflow : outs ) {
        for ( auto &inflow : ins ) {            
            if (stores_equal(outflow.store, inflow.store)) {
                if (!processes_equal(outflow.process, inflow.process)) {
		    // printf ("\ninflow matches outflow\n");
		    //printf (" -- processes differ \n");
		    //print_flow(outflow); printf ("\n");
		    //print_flow(inflow); printf ("\n");
		    //printf("\n");
		    NamePid out = NamePid(outflow.process.name, outflow.process.pid);
		    NamePid in = NamePid(inflow.process.name, inflow.process.pid);
		    proc_comm_count[out][in]++;
		    proc_comm_type_count[out][in][outflow.store.type]++;
                }
                else {
                    //printf (" -- processes same\n");
                }
            }
        }
    }
    for (auto &fullflow : fullflows ) {
        //print_full_flow(fullflow); printf("\n");
        NamePid sender = NamePid(fullflow.sender.name, fullflow.sender.pid);
        NamePid target =  NamePid(fullflow.target.name, fullflow.target.pid);
        if (fullflow.dir == Out) {
            proc_comm_count[sender][target]++;
            proc_comm_type_count[sender][target][fullflow.store.type]++;
        } else if (fullflow.dir == In){
            proc_comm_count[target][sender]++;
            proc_comm_type_count[target][sender][fullflow.store.type]++;
        } else {
            proc_comm_type_count[sender][target][fullflow.store.type]++;
            proc_comm_type_count[target][sender][fullflow.store.type]++;
            proc_comm_count[sender][target]++;
            proc_comm_count[target][sender]++;
        }
    }}



int main (int argc, char **argv) {
    pandalog_open(argv[1], "r");
    Panda__LogEntry *ple;
    while (1) {
        ple = pandalog_read_entry();
	if (ple == (Panda__LogEntry *)1){
	    continue;
	}
        if (ple == NULL) {
            break;
        }
        if (ple->new_pid) { 
	    NamePid p = NamePid(strdup(ple->new_pid->name), ple->new_pid->pid);
	    procs.insert(p);
        }
#if 0
        else if (ple->nt_create_user_process) {           
	    Process p = {ple->nt_create_user_process->new_p->pid, strdup(ple->nt_create_user_process->new_p->name)};
	    bool unique = true;
	    for (auto &proc : procs) {
		if (processes_equal(p, proc)){
		    unique = false;
		    break;
		}
	    }
	    if (unique){
		procs.push_back(p);
	    }
        }
        else if (ple->nt_terminate_process) {
        }
#endif
#if 0
        else if (ple->nt_create_file) {
        }
#endif 
        else if (ple->nt_read_file) {
	    //printf("nt_read_file\n");
            Flow inflow;
            inflow.instr = ple->instr;
            inflow.pc = ple->pc;
            inflow.dir = In;
            inflow.process = {ple->nt_read_file->proc->pid, strdup(ple->nt_read_file->proc->name)};
            inflow.store.type = File;
            inflow.store.val.fd.filename = strdup(ple->nt_read_file->filename);
            ins.push_back(inflow);
        }
        else if (ple->nt_delete_file) {
        }
        else if (ple->nt_write_file) {
	    //printf("nt_write_file\n");
            Flow outflow;
            outflow.instr = ple->instr;
            outflow.pc = ple->pc;
            outflow.dir = Out;
            outflow.process = {ple->nt_write_file->proc->pid, strdup(ple->nt_write_file->proc->name)};
            outflow.store.type = File;
            outflow.store.val.fd.filename = strdup(ple->nt_write_file->filename);
            outs.push_back(outflow);
        }
        else if (ple->nt_create_key) {
	    //ProcessKey
        }
        else if (ple->nt_create_key_transacted) {
	    //ProcessKey
        }
        else if (ple->nt_open_key) {
	    //ProcessKey
        }
        else if (ple->nt_open_key_ex) {
	    //ProcessKey
        }
        else if (ple->nt_open_key_transacted) {
	    //ProcessKey
        }
        else if (ple->nt_open_key_transacted_ex) {
	    //ProcessKey
        }
        else if (ple->nt_delete_key) {
	    //ProcessKey
	    //Outflow
        }
        else if (ple->nt_query_key) {
	    //ProcessKey
	    //Inflow
        }
        else if (ple->nt_query_value_key) {
	    //ProcessKeyValue
	    //Inflow
	    //printf("nt_query_value_key\n");
	    Flow flow;
	    char *value = strdup(ple->nt_query_value_key->value_name);
	    char *key = strdup(ple->nt_query_value_key->pk->keyname);
	    flow.instr = ple->instr;
	    flow.dir = In;
	    flow.process = {ple->nt_query_value_key->pk->proc->pid, strdup(ple->nt_query_value_key->pk->proc->name)};
	    flow.store.type = RegKeyVal;
	    flow.store.val.kvd.keyname = key;
	    flow.store.val.kvd.valname = value;
        }
        else if (ple->nt_delete_value_key) {
	    //ProcessKeyValue
	    //Outflow
	    //printf("nt_delete_value_key\n");
            Flow flow;
            char *value = strdup(ple->nt_delete_value_key->value_name);
            char *key = strdup(ple->nt_delete_value_key->pk->keyname);
            flow.instr = ple->instr;
            flow.dir = Out;
	    flow.process = {ple->nt_delete_value_key->pk->proc->pid, strdup(ple->nt_delete_value_key-> pk->proc->name)};
            flow.store.type = RegKeyVal;
            flow.store.val.kvd.keyname = key;
            flow.store.val.kvd.valname = value;
        }
        else if (ple->nt_set_value_key) {
	    //printf("nt_set_value_key\n");
	    //ProcessKeyValue
	    //Outflow
            Flow flow;
            char *value = strdup(ple->nt_set_value_key->value_name);
            char *key = strdup(ple->nt_set_value_key->pk->keyname);
            flow.instr = ple->instr;
            flow.dir = Out;
            flow.process = {ple->nt_set_value_key->pk->proc->pid, strdup(ple->nt_set_value_key-> pk->proc->name)};
            flow.store.type = RegKeyVal;
            flow.store.val.kvd.keyname = key;
            flow.store.val.kvd.valname = value;
        }
        else if (ple->nt_enumerate_key) {
	    //ProcessKeyIndex
	    //Inflow
        }
        else if (ple->nt_enumerate_value_key) {
	    //ProcessKeyIndex
	    //Inflow
        }else if (ple->nt_create_section) {
	    //printf("nt_create_section\n");
	    Flow outflow;
            outflow.instr = ple->instr;
            outflow.pc = ple->pc;
            outflow.dir = Out;
            outflow.process = {ple->nt_create_section->proc->pid, strdup(ple->nt_create_section->proc->name)};
            outflow.store.type = Section;
	    outflow.store.val.sd.id = ple->nt_create_section->section_id;
	    if (ple->nt_create_section->name != NULL){
		outflow.store.val.sd.name = strdup(ple->nt_create_section->name);
	    }
	    if (ple->nt_create_section->file_name != NULL){
		outflow.store.val.sd.file_name = strdup(ple->nt_create_section->file_name);	    
		Flow inflow;
		inflow.instr = ple->instr;
		inflow.pc = ple->pc;
		inflow.dir = In;
		inflow.process = {ple->nt_create_section->proc->pid, strdup(ple->nt_create_section->proc->name)};
		inflow.store.type = File;
		inflow.store.val.fd.filename = strdup(ple->nt_create_section->file_name);
		ins.push_back(inflow);
	    }
	    outs.push_back(outflow);
	    Flow f;
	    f.dir = In;
            f.instr = ple->instr;
            f.pc = ple->pc;
	    f.process = {ple->nt_create_section->proc->pid, strdup(ple->nt_create_section->proc->name)};
            f.store.type = Section;
            f.store.val.sd.id = ple->nt_create_section->section_id;
            if (ple->nt_create_section->name != NULL){
                f.store.val.sd.name = strdup(ple->nt_create_section->name);
            }
	    ins.push_back(f);
        }else if (ple->nt_open_section) {
	    //printf("nt_open_section\n");
	    Flow outflow;
	    outflow.instr = ple->instr;
	    outflow.pc = ple->pc;
	    //outflow.dir = Out;
	    outflow.process = {ple->nt_open_section->proc->pid, strdup(ple->nt_open_section->proc->name)};
	    outflow.store.type = Section;
	    outflow.store.val.sd.id = ple->nt_open_section->section_id;
	    if (ple->nt_open_section->name){
                outflow.store.val.sd.name = strdup(ple->nt_open_section->name);
	    }
	    if (ple->nt_open_section->file_name){
                outflow.store.val.sd.file_name = strdup(ple->nt_open_section->file_name);
		Flow inflow;
		inflow.instr = ple->instr;
		inflow.pc = ple->pc;
		inflow.dir = In;
		inflow.process = {ple->nt_open_section->proc->pid, strdup(ple->nt_open_section->proc->name)};
		inflow.store.type = File;
		inflow.store.val.fd.filename = strdup(ple->nt_open_section->file_name);
		ins.push_back(inflow);
	    }
	    outs.push_back(outflow);
	    Flow f;
            f.dir = In;
            f.instr = ple->instr;
            f.pc = ple->pc;
            f.dir = Out;
            f.process = {ple->nt_open_section->proc->pid, strdup(ple->nt_open_section->proc->name)};
            f.store.type = Section;
            f.store.val.sd.id = ple->nt_open_section->section_id;
            if (ple->nt_open_section->name != NULL){
                f.store.val.sd.name = strdup(ple->nt_open_section->name);
            }
        }else if (ple->nt_map_view_of_section) {
	    //printf("nt_map_view_of_section\n");
	    //TODO: make this a full flow. Since it really is.
	    Flow inflow;
            inflow.instr = ple->instr;
            inflow.pc = ple->pc;
            inflow.dir = In;
            inflow.process = {ple->nt_map_view_of_section->target->pid, strdup(ple->nt_map_view_of_section->target->name)};
            inflow.store.type = Section;
            inflow.store.val.sd.id = ple->nt_map_view_of_section->section->section_id;
            ins.push_back(inflow);
        }else if (ple->nt_create_port) {
	    Panda__LocalPort *port = ple->nt_create_port->port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_connect_port) {
	    Panda__LocalPort *port = ple->nt_connect_port->port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_listen_port) {
	    Panda__LocalPort *port = ple->nt_listen_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_accept_connect_port) {
	    Panda__LocalPort *port = ple->nt_accept_connect_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_complete_connect_port) {
	    Panda__LocalPort *port = ple->nt_complete_connect_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_request_port) {
	    Panda__LocalPort *port = ple->nt_request_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_request_wait_reply_port) {
	    Panda__LocalPort *port = ple->nt_request_wait_reply_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_reply_port) {
	    Panda__LocalPort *port = ple->nt_reply_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_reply_wait_reply_port) {
	    Panda__LocalPort *port = ple->nt_reply_wait_reply_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_reply_wait_receive_port) {
	    Panda__LocalPort *port = ple->nt_reply_wait_receive_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
        }else if (ple->nt_impersonate_client_of_port) {
	    Panda__LocalPort *port = ple->nt_impersonate_client_of_port;
	    if (port != NULL){
		if ((port->client != NULL)
		    && (port->server != NULL)){
		    FullFlow flow;
		    flow.instr = ple->instr;
		    flow.pc = ple->pc;
		    flow.dir = Both;
		    flow.sender = {port->client->pid, strdup(port->client->name)};
		    flow.target = {port->server->pid, strdup(port->server->name)};
		    flow.store.type = LocalPort;
		    flow.store.val.lpd.id = port->id;
		    flow.store.val.lpd.client = {port->client->pid, strdup(port->client->name)};
		    flow.store.val.lpd.server = {port->server->pid, strdup(port->server->name)};
		    fullflows.push_back(flow);
		}
	    }
	} else if (ple->nt_read_virtual_memory) {
	    FullFlow flow;
	    flow.instr = ple->instr;
	    flow.pc = ple->pc;
	    flow.dir = In;
	    flow.sender = {ple->nt_read_virtual_memory->proc->pid, strdup(ple->nt_read_virtual_memory->proc->name)};
	    flow.target = {ple->nt_read_virtual_memory->target->pid, strdup(ple->nt_read_virtual_memory->target->name)};
	    flow.store.type = VM;
	    fullflows.push_back(flow);
	} else if (ple->nt_write_virtual_memory) {
            FullFlow flow;
            flow.instr = ple->instr;
            flow.pc = ple->pc;
            flow.dir = Out;
            flow.sender = {ple->nt_write_virtual_memory->proc->pid, strdup(ple->nt_write_virtual_memory->proc->name)};
	    flow.target = {ple->nt_write_virtual_memory->target->pid, strdup(ple->nt_write_virtual_memory->target->name)};
            flow.store.type = VM;
            fullflows.push_back(flow);
        } else {
            //printf ("unrecognized!\n");
        }

    //printf ("\n");
        panda__log_entry__free_unpacked(ple, NULL);
    }
    flow_match();
    /*
    for (auto &kvp : proc_comm_count){
	print_namepid(kvp.first);
	printf (" : ");
	for (auto &kvp2 : kvp.second) {
	    printf("\n  [");
	    print_namepid(kvp2.first);
	    printf(": %d]", kvp2.second);
	}
	printf("\n");
    }
    */
    int total = 0;
    for (auto &kvp1 : proc_comm_type_count) {
	//Nampid -> Namepid -> storetype -> count
	print_namepid(kvp1.first);
	for (auto &kvpz : proc_comm_count[kvp1.first]) {
	    total += kvpz.second;
	}
	printf (" : %d\n", total);
	for (auto &kvp2 : kvp1.second) {
	    printf("  ");
	    print_namepid(kvp2.first);
	    printf(" : {");
	    for (auto &kvp3 : kvp2.second) {
		printf("[%s", storename[kvp3.first]);
		printf(", %d]", kvp3.second);
	    }
	    printf("}\n");
	}
	total = 0;
    }
}
