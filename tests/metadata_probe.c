#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <json-c/json.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
static int fail_rename;
/* Match POSIX replacement for these regular-file host fixtures. */
static int host_rename(const char *a,const char *b) {
    if(!fail_rename&&MoveFileExA(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return 0;
    errno=EACCES;return -1;
}
#define rename host_rename
#define stat _stat64
#define lstat _stat64
#define mkdir(path,mode) _mkdir(path)
#define fsync _commit
#endif
#include "../src/pkg_reader.h"
#define STATE "state"
static FILE *logfile;
static unsigned created;
static int directory(const char *path) {
    struct stat st;
    if(!lstat(path,&st)){if(S_ISDIR(st.st_mode))return 1;errno=ENOTDIR;return 0;}
    return errno==ENOENT&&!mkdir(path,0777);
}
#include "../src/restore_metadata.h"
int main(int argc,char **argv) {
    if(argc!=4)return 2;
    _set_fmode(_O_BINARY);
    FILE *f=fopen(argv[2],"rb");if(!f)return 2;
    PKG_SEEK(f,0,SEEK_END);
    RestoreInput in={f,0,(uint64_t)_ftelli64(f)};
    logfile=fopen("restore.log","ab");if(!logfile)return 2;
    if(!directory(STATE))return 2;
    int is_param=strcmp(argv[1],"blob")!=0;
    struct json_object *expected=is_param?restore_json(&in):NULL;
    if(is_param&&!restore_title_matches(expected,"PPSA08668")&&!restore_title_matches(expected,"PPSA01467"))return 3;
    const char *title=is_param?json_object_get_string(json_object_object_get(expected,"titleId")):"PPSA08668";
    int ok=restore_stage(argv[3],&in,0,is_param,expected,title,"test");
    if(!strcmp(argv[1],"fail-rename"))fail_rename=1;
    if(ok&&strcmp(argv[1],"preflight"))ok=restore_stage(argv[3],&in,1,is_param,expected,title,"test");
    printf("success=%d created=%u updated=%u\n",ok,created,updated);
    if(expected)json_object_put(expected);
    fclose(f);fclose(logfile);return ok?0:1;
}
