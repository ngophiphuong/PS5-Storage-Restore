/* PS5 Storage Restore 1.3 EN/VI. Existing installed FIH PKGs only.
 * Copyright (c) 2026 NGÔ PHI PHƯƠNG x PSVIETHOA.COM
 * SPDX-License-Identifier: GPL-3.0-only */
#include "pkg_reader.h"
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <dlfcn.h>
#include <ps5/kernel.h>
#include <json-c/json.h>
#define STATE "/data/ps5_storage_restore"
extern int sceKernelSendNotificationRequest(int,void *,size_t,int);
extern int32_t sceSystemServiceParamGetInt(int32_t,int32_t *);
static FILE *logfile;
static int vietnamese;
static const char *tr(const char *vi,const char *en){return vietnamese?vi:en;}
static unsigned found,ok,skipped,created;
static int (*register_title)(const char *,const char *,void *);
static void notice(const char *s) {
    unsigned char b[3120]={0};snprintf((char *)b+45,sizeof(b)-45,
        "PS5 STORAGE RESTORE\n%s\n%s NGÔ PHI PHƯƠNG x PSVIETHOA.COM",
        s,tr("Bản quyền","Copyright"));
    sceKernelSendNotificationRequest(0,b,sizeof(b),0);
}
static int directory(const char *path) {
    struct stat st;
    if(!lstat(path,&st)){if(S_ISDIR(st.st_mode))return 1;errno=ENOTDIR;return 0;}
    return errno==ENOENT&&!mkdir(path,0777);
}
static int appmeta_item(const char *name) {
    if(strchr(name,'/'))return 0;
    if(!strcmp(name,"param.json"))return 1;
    const char *ext=strrchr(name,'.');
    return ext&&(!strcmp(ext,".png")||!strcmp(ext,".dds")||!strcmp(ext,".at9"));
}
static int trophy_item(const char *name) {
    return !strncmp(name,"trophy2/",8)||!strncmp(name,"uds/",4)||!strcmp(name,"param.json");
}
static int empty_dir(const char *path) {
    DIR *d=opendir(path);if(!d)return 0;struct dirent *e;int empty=1;
    while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,"..")){empty=0;break;}
    closedir(d);return empty;
}
static int same_file(const char *path,const unsigned char *b,size_t n) {
    struct stat st;if(lstat(path,&st)||!S_ISREG(st.st_mode)||(uint64_t)st.st_size!=n)return 0;
    FILE *f=fopen(path,"rb");if(!f)return 0;
    unsigned char buf[16384];size_t pos=0;int same=1;
    while(pos<n){size_t k=n-pos;if(k>sizeof(buf))k=sizeof(buf);
        if(fread(buf,1,k,f)!=k||memcmp(buf,b+pos,k)){same=0;break;}pos+=k;}
    fclose(f);return same;
}
/* Preserve existing artwork; param.json and ownership records must match. */
static int stage_file(const char *path,const unsigned char *b,size_t n,int write_it,int keep_existing) {
    struct stat st;
    if(!lstat(path,&st))return S_ISREG(st.st_mode)&&(keep_existing||same_file(path,b,n));
    if(errno!=ENOENT)return 0;
    if(!write_it)return 1;
    char temp[640];snprintf(temp,sizeof(temp),"%s.restore-%d.tmp",path,getpid());
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0644);if(fd<0)return 0;
    size_t pos=0;int success=1;
    while(pos<n){ssize_t k=write(fd,b+pos,n-pos);if(k<0&&errno==EINTR)continue;
        if(k<=0){success=0;break;}pos+=(size_t)k;}
    if(success&&fsync(fd))success=0;
    if(close(fd))success=0;
    if(success&&!same_file(temp,b,n))success=0;
    if(success) {
        if(!lstat(path,&st))success=S_ISREG(st.st_mode)&&(keep_existing||same_file(path,b,n));
        else if(errno!=ENOENT||rename(temp,path))success=0;
        else {created++;fprintf(logfile,"CREATED %s bytes=%zu\n",path,n);}
    }
    unlink(temp);return success;
}
#include "restore_metadata.h"
static int marker_matches(const char *marker,const char *source) {
    char b[320];FILE *f=fopen(marker,"rb");if(!f)return 0;
    size_t n=fread(b,1,sizeof(b)-1,f);fclose(f);b[n]=0;
    if(n&&b[n-1]=='\n')b[--n]=0;return !strcmp(b,source);
}
static void process_title(const char *root,const char *id,int internal) {
    char source[256],dest[256],pkgpath[320],meta[320],appmeta[256],sysmeta[320],marker[256];
    struct stat st;struct statfs fs;int new_dir=0,new_mount=0,success=0;
    snprintf(source,sizeof(source),"%s/%s",root,id);snprintf(dest,sizeof(dest),"/user/app/%s",id);
    snprintf(pkgpath,sizeof(pkgpath),"%s/app.pkg",source);
    snprintf(meta,sizeof(meta),"%s/sce_sys",source);snprintf(appmeta,sizeof(appmeta),"/user/appmeta/%s",id);
    snprintf(sysmeta,sizeof(sysmeta),"/system_data/priv/appmeta/%s",id);
    snprintf(marker,sizeof(marker),STATE "/owned/%s",id);
    if(lstat(source,&st)||!S_ISDIR(st.st_mode))return;
    /* Internal scan ignores aliases already handled through the actual device. */
    if(internal&&!statfs(source,&fs)&&!strcmp(fs.f_fstypename,"nullfs"))return;
    if(lstat(pkgpath,&st)||!S_ISREG(st.st_mode))return;
    found++;fprintf(logfile,"FOUND %s source=%s bytes=%llu\n",id,source,(unsigned long long)st.st_size);fflush(logfile);
    Package p={0};struct json_object *param_obj=NULL;
    p.size=(uint64_t)st.st_size;p.file=fopen(pkgpath,"rb");
    if(!p.file){fprintf(logfile,"SKIP open PKG %s errno=%d\n",id,errno);goto done;}
    if(!pkg_parse(&p)){fprintf(logfile,"SKIP invalid/unsupported PKG table %s reason=%s\n",id,p.error);goto done;}
    for(unsigned i=0;i<p.count;i++)if(!strcmp(p.items[i].name,"param.json")) {
        RestoreInput in={p.file,p.items[i].offset,p.items[i].size};param_obj=restore_json(&in);break;
    }
    if(!restore_title_matches(param_obj,id)){fprintf(logfile,"SKIP invalid param.json/titleId %s\n",id);goto done;}
    int same_mount=!internal&&!statfs(dest,&fs)&&!strcmp(fs.f_fstypename,"nullfs")&&
        !strcmp(fs.f_mntonname,dest)&&!strcmp(fs.f_mntfromname,source);
    int mounted=same_mount&&!(fs.f_flags&MNT_RDONLY);
    if(same_mount&&!mounted) {
        if(unmount(dest,0)){fprintf(logfile,"SKIP cannot replace read-only mount %s errno=%d\n",id,errno);goto done;}
        fprintf(logfile,"REMOUNT read-only alias removed %s\n",id);
    }
    if(!internal&&!mounted) {
        if(!lstat(dest,&st)) {
            if(!S_ISDIR(st.st_mode)||!marker_matches(marker,source)||!empty_dir(dest)) {
                fprintf(logfile,"SKIP occupied/unowned destination %s\n",dest);goto done;
            }
        }else if(errno!=ENOENT){fprintf(logfile,"SKIP destination inaccessible %s\n",dest);goto done;}
    }
    char meta_trophy[384],meta_uds[384],sys_trophy[384],sys_uds[384];
    snprintf(meta_trophy,sizeof(meta_trophy),"%s/trophy2",meta);
    snprintf(meta_uds,sizeof(meta_uds),"%s/uds",meta);
    snprintf(sys_trophy,sizeof(sys_trophy),"%s/trophy2",sysmeta);
    snprintf(sys_uds,sizeof(sys_uds),"%s/uds",sysmeta);
    if(!directory(meta)||!directory(appmeta)||!directory("/system_data/priv/appmeta")||
       !directory(sysmeta)||!directory(meta_trophy)||!directory(meta_uds)||
       !directory(sys_trophy)||!directory(sys_uds)){
        fprintf(logfile,"SKIP metadata directory unavailable %s\n",id);goto done;
    }
    /* Preflight all destinations before copying; large files use bounded buffers. */
    for(int pass=0;pass<2;pass++)for(unsigned i=0;i<p.count;i++) {
        PkgItem *item=&p.items[i];RestoreInput in={p.file,item->offset,item->size};char a[512],c[512],d[512];
        snprintf(a,sizeof(a),"%s/%s",meta,item->name);
        snprintf(c,sizeof(c),"%s/%s",appmeta,item->name);
        snprintf(d,sizeof(d),"%s/%s",sysmeta,item->name);
        int is_param=!strcmp(item->name,"param.json");
        int good=restore_stage(a,&in,pass,is_param,param_obj,id,"source")&&
            (!appmeta_item(item->name)||restore_stage(c,&in,pass,is_param,param_obj,id,"appmeta"))&&
            (!trophy_item(item->name)||restore_stage(d,&in,pass,is_param,param_obj,id,"sysmeta"));
        if(!good){fprintf(logfile,"SKIP metadata failed %s %s (see METADATA_FAILED)\n",id,item->name);goto done;}
    }
    if(!internal) {
        /* Record ownership before creating an alias; never replace another source. */
        char line[320];snprintf(line,sizeof(line),"%s\n",source);
        if(!stage_file(marker,(unsigned char *)line,strlen(line),1,0)) {fprintf(logfile,"SKIP marker conflict/write failure %s errno=%d\n",id,errno);goto done;}
        if(!mounted) {
            if(lstat(dest,&st)) {
                if(errno!=ENOENT||mkdir(dest,0777)){fprintf(logfile,"SKIP mkdir %s errno=%d\n",id,errno);goto done;}
                new_dir=1;
            }
            char *v[]={"fstype","nullfs","from",source,"fspath",dest};struct iovec iov[6];
            for(unsigned i=0;i<6;i++){iov[i].iov_base=v[i];iov[i].iov_len=strlen(v[i])+1;}
            if(nmount(iov,6,0)){fprintf(logfile,"MOUNT_FAILED %s errno=%d\n",id,errno);goto done;}
            new_mount=1;
        }
    }
    int rc=register_title(id,"/user/app/",NULL);
    fprintf(logfile,"REGISTER %s rc=0x%08x mode=%s\n",id,rc,internal?"internal-direct":"external-nullfs");
    success=rc==0||(unsigned)rc==0x80990002u;
done:
    if(param_obj)json_object_put(param_obj);
    if(p.file)fclose(p.file);
    if(success)ok++;
    else {
        skipped++;
        if(new_mount&&unmount(dest,0)){fprintf(logfile,"ROLLBACK unmount failed %s errno=%d\n",dest,errno);new_dir=0;}
        if(new_dir)rmdir(dest);
    }
    fflush(logfile);
}
static void scan(const char *root,int internal) {
    struct stat st;if(lstat(root,&st)||!S_ISDIR(st.st_mode))return;
    DIR *d=opendir(root);if(!d)return;
    fprintf(logfile,"SCAN %s\n",root);struct dirent *e;
    while((e=readdir(d)))if(title_valid(e->d_name))process_title(root,e->d_name,internal);
    closedir(d);
}
int main(void) {
    int32_t system_language=-1;
    int lang_rc=sceSystemServiceParamGetInt(1,&system_language);
    vietnamese=lang_rc==0&&system_language==28;
    if(!directory(STATE)||!directory(STATE "/owned")){notice(tr("Không tạo được thư mục lưu trạng thái.","Cannot create state directory."));return 1;}
    int lock=open(STATE "/running.lock",O_RDWR|O_CREAT,0600);
    if(lock<0||flock(lock,LOCK_EX|LOCK_NB)){if(lock>=0)close(lock);notice(tr("Đang có phiên chạy khác hoặc không khóa được phiên.","Another instance is running or the session lock is unavailable."));return 1;}
    logfile=fopen(STATE "/restore.log","a");if(!logfile){close(lock);notice(tr("Không mở được file nhật ký.","Cannot open log file."));return 1;}
    fprintf(logfile,"START version=1.3-EN-VI-streaming pid=%d\nCopyright NGÔ PHI PHƯƠNG x PSVIETHOA.COM\nLANG system=%d rc=0x%08x selected=%s\n",getpid(),system_language,lang_rc,vietnamese?"vi":"en");fflush(logfile);
    notice(tr("Đang quét game trên bộ nhớ trong và SSD M.2...","Scanning games on internal storage and M.2 SSD..."));
    uint64_t prior=kernel_get_ucred_authid(-1);kernel_set_ucred_authid(-1,0x4800000000000006ULL);
    void *h=dlopen("/system/common/lib/libSceAppInstUtil.sprx",RTLD_LAZY);
    int (*init)(void)=h?dlsym(h,"sceAppInstUtilInitialize"):NULL;
    register_title=h?dlsym(h,"sceAppInstUtilAppInstallTitleDir"):NULL;
    int result=1;
    if(!init||!register_title) {fprintf(logfile,"ERROR AppInst API unavailable\n");notice(tr("Không nạp được AppInst. Xem file nhật ký.","Cannot load AppInst. Check the log."));goto finish;}
    fprintf(logfile,"INIT rc=0x%08x\n",init());
    if(!directory("/user/app")||!directory("/user/appmeta")){fprintf(logfile,"ERROR app directories unavailable\n");notice(tr("Không truy cập được thư mục game hệ thống.","Cannot access system application directories."));goto finish;}
    /* Direct internal installs win if a title also exists on another drive. */
    scan("/user/app",1);
    for(unsigned n=0;n<16;n++){char root[64];snprintf(root,sizeof(root),"/mnt/ext%u/user/app",n);scan(root,0);}
    char message[256];snprintf(message,sizeof(message),tr(
        "Đã đăng ký %u/%u game/ứng dụng. Bỏ qua: %u.\nMở Thư viện trò chơi để kiểm tra.",
        "Registered %u/%u games/apps. Skipped: %u.\nOpen Game Library to check."),ok,found,skipped);notice(message);
    result=skipped?2:0;
finish:
    kernel_set_ucred_authid(-1,prior);
    fprintf(logfile,"END registered=%u found=%u skipped=%u created=%u updated=%u\n",ok,found,skipped,created,updated);
    fclose(logfile);close(lock);return result;
}
