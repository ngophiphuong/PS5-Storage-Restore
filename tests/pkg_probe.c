#include "../src/pkg_reader.h"
int main(int argc,char **argv) {
    if(argc!=2&&argc!=4)return 2;
    Package p={0};p.file=fopen(argv[1],"rb");if(!p.file)return 2;
    PKG_SEEK(p.file,0,SEEK_END);
#ifdef _WIN32
    p.size=(uint64_t)_ftelli64(p.file);
#else
    p.size=(uint64_t)ftello(p.file);
#endif
    if(argc==4)p.size=strtoull(argv[3],NULL,10);
    if(!pkg_parse(&p)){fprintf(stderr,"%s\n",p.error);fclose(p.file);return 1;}
    if(!title_valid("PPSA12345")||title_valid("PPSA1234/")||title_valid("CUSA12345"))return 3;
    for(unsigned i=0;i<p.count;i++) {
        if(argc==2) {
            unsigned char b[65536];
            for(uint64_t off=0;off<p.items[i].size;) {
                size_t n=p.items[i].size-off>sizeof(b)?sizeof(b):(size_t)(p.items[i].size-off);
                if(!pkg_read(&p,p.items[i].offset+off,b,n))return 3;off+=n;
            }
        }
        printf("%s %u\n",p.items[i].name,p.items[i].size);
    }
    fclose(p.file);return 0;
}
