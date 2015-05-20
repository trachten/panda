
// cd panda/qemu
// g++ -g -o pandalog_reader pandalog_reader.cpp pandalog.c pandalog.pb-c.c ../../../lava/src_clang/lavaDB.cpp  -L/usr/local/lib -lprotobuf-c -I .. -lz -D PANDALOG_READER  -std=c++11 


#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pandalog.h"
#include "pandalog_print.h"
#include <map>
#include <string>

//#include "../../../lava/src_clang/lavaDB.h"

//std::map<std::string,uint32_t> str2ind;
//std::map<uint32_t,std::string> ind2str;

/*
char *gstr(uint32_t ind) {
    return (char *) (ind2str[ind].c_str());
}
*/
int main (int argc, char **argv) {
    
  //str2ind = LoadDB(std::string("/tmp/lavadb"));
  //ind2str = InvertDB(str2ind);
    
    pandalog_open(argv[1], "r");
    Panda__LogEntry *ple;
    while (1) {
        ple = pandalog_read_entry();
	if (ple == (Panda__LogEntry *)1) {
	    continue;
	}
        if (ple == NULL) {
	    break;
        }
	print_ple(ple);
	panda__log_entry__free_unpacked(ple, NULL);
    }
}
