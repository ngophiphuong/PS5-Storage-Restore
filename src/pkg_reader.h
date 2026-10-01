/* Copyright (c) 2026 NGÔ PHI PHƯƠNG x PSVIETHOA.COM
 * SPDX-License-Identifier: GPL-3.0-only */
#ifndef PKG_READER_H
#define PKG_READER_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#define PKG_SEEK _fseeki64
#else
#define PKG_SEEK fseeko
#endif
typedef struct { char name[128]; uint64_t offset; uint32_t size; } PkgItem;
typedef struct { FILE *file; uint64_t size; PkgItem items[512]; unsigned count; char error[256]; } Package;
static int pkg_error(Package *p,const char *format,...) {
    va_list args;va_start(args,format);vsnprintf(p->error,sizeof(p->error),format,args);va_end(args);return 0;
}
static uint32_t be32(const unsigned char *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static uint64_t le64(const unsigned char *p) {
    uint64_t v=0; for(unsigned i=0;i<8;i++) v|=(uint64_t)p[i]<<(8*i); return v;
}
static int pkg_read(Package *p,uint64_t offset,void *out,size_t size) {
    return offset<=p->size && size<=p->size-offset && offset<=INT64_MAX &&
        !PKG_SEEK(p->file,(int64_t)offset,SEEK_SET) && fread(out,1,size,p->file)==size;
}
static int title_valid(const char *s) {
    if(strlen(s)!=9 || memcmp(s,"PPSA",4)) return 0;
    for(unsigned i=4;i<9;i++) if(s[i]<'0'||s[i]>'9') return 0;
    return 1;
}
static int wanted_name(const char *s) {
    size_t n=strlen(s); if(!n || n>=128 || s[0]=='.') return 0;
    const char *slash=strchr(s,'/');
    if(slash) {
        if(strchr(slash+1,'/')||
           (strncmp(s,"trophy2/",8)&&strncmp(s,"uds/",4))) return 0;
    }
    for(size_t i=0;i<n;i++) {
        unsigned char c=(unsigned char)s[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/')) return 0;
    }
    if(!strcmp(s,"param.json")||!strcmp(s,"playgo-scenario.json")||
       !strcmp(s,"playgo-chunk.dat")||!strcmp(s,"playgo-hash-table.dat")||
       !strcmp(s,"playgo-ficm.dat")||!strcmp(s,"trophy2/npbind.dat")||
       !strcmp(s,"uds/npbind.dat")) return 1;
    const char *ext=strrchr(s,'.');
    if(slash) return ext&&!strcmp(ext,".ucp");
    return ext && (!strcmp(ext,".png")||!strcmp(ext,".dds")||!strcmp(ext,".at9"));
}
/* Parse only bounded, plaintext metadata. No executable or encrypted content. */
static int pkg_parse(Package *p) {
    unsigned char h[256], c[128]; unsigned char *table=NULL,*names=NULL;
    int result=0; p->count=0;p->error[0]=0;
#define PKG_REJECT(...) do { pkg_error(p,__VA_ARGS__);goto done; } while(0)
    if(!pkg_read(p,0,h,sizeof(h))||memcmp(h,"\177FIH",4)) return pkg_error(p,"invalid/unreadable FIH header");
    uint64_t base=le64(h+0x58);
    if(base<sizeof(h)||base>p->size||!pkg_read(p,base,c,sizeof(c))||memcmp(c,"\177CNT",4)) return pkg_error(p,"invalid/unreadable CNT header");
    uint32_t count=be32(c+0x10), toff=be32(c+0x18);
    if(!count||count>65535||toff<sizeof(c)||toff>p->size-base) return pkg_error(p,"invalid CNT table count/offset");
    size_t bytes=(size_t)count*32;
    table=malloc(bytes); if(!table||!pkg_read(p,base+toff,table,bytes)) PKG_REJECT("cannot read/allocate CNT table");
    uint32_t noff=0,nsize=0; unsigned name_tables=0;
    for(uint32_t i=0;i<count;i++) {
        unsigned char *e=table+i*32; uint32_t off=be32(e+16),size=be32(e+20);
        if(off>p->size-base||size>p->size-base-off) PKG_REJECT("entry %u outside PKG: offset=%u size=%u",i,off,size);
        if(be32(e)==0x200) {
            if(++name_tables!=1||(be32(e+8)&~0x48000000u)||be32(e+12)) PKG_REJECT("duplicate/unsupported name table");
            noff=off;nsize=size;
        }
    }
    if(name_tables!=1||!nsize) PKG_REJECT("missing/empty name table");
    names=malloc(nsize); if(!names||!pkg_read(p,base+noff,names,nsize)) PKG_REJECT("cannot read/allocate name table: bytes=%u",nsize);
    unsigned param=0;
    for(uint32_t i=0;i<count;i++) {
        unsigned char *e=table+i*32; uint32_t ni=be32(e+4),size=be32(e+20);
        if(!ni) continue;
        if(ni>=nsize||!memchr(names+ni,0,nsize-ni)) PKG_REJECT("entry %u invalid filename offset",i);
        const char *name=(const char *)names+ni;
        if(!wanted_name(name)) continue;
        if((be32(e+8)&~0x08000000u)||be32(e+12)) {
            if(!strcmp(name,"param.json")) PKG_REJECT("param.json has unsupported flags: %08x/%08x",be32(e+8),be32(e+12));
            continue;
        }
        if(!size||p->count>=512) PKG_REJECT("invalid size or too many metadata entries: %s size=%u count=%u",name,size,p->count);
        for(unsigned j=0;j<p->count;j++) if(!strcmp(name,p->items[j].name)) PKG_REJECT("duplicate metadata: %s",name);
        PkgItem *item=&p->items[p->count++]; strcpy(item->name,name);
        item->size=size;item->offset=base+be32(e+16);
        if(!strcmp(name,"param.json")) param++;
    }
    result=param==1;if(!result)pkg_error(p,"missing param.json");
done:
    free(table);free(names);if(!result)p->count=0;return result;
#undef PKG_REJECT
}
#endif
