#define NOB_IMPLEMENTATION
#include "nob.h"


int main(int argc, char **argv){
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "gcc", "-O3", "-march=native", "-Wall", "-Wextra", "-g","-I","./libmill/", "-L", "./libmill/build", "-o", "main", "main.c", "-l:libmill.a");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}