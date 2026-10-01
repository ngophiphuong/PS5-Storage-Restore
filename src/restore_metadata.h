/* Copyright (c) 2026 NGÔ PHI PHƯƠNG x PSVIETHOA.COM
 * SPDX-License-Identifier: GPL-3.0-only */
#ifndef RESTORE_METADATA_H
#define RESTORE_METADATA_H
#include <ctype.h>

#define RESTORE_CHUNK 65536u
typedef struct { FILE *file; uint64_t offset,size; } RestoreInput;
static unsigned updated,backup_serial,temp_serial;

static int restore_fail(const char *path,const char *reason,int code) {
    fprintf(logfile,"METADATA_FAILED path=%s reason=%s errno=%d\n",path,reason,code);
    errno=code;return 0;
}
static int restore_read(const RestoreInput *in,uint64_t off,void *buf,size_t n) {
    if(off>in->size||n>in->size-off||in->offset>INT64_MAX||off>(uint64_t)INT64_MAX-in->offset) {
        errno=EINVAL;return 0;
    }
    errno=0;
    if(PKG_SEEK(in->file,(int64_t)(in->offset+off),SEEK_SET)||fread(buf,1,n,in->file)!=n) {
        if(!errno)errno=EIO;return 0;
    }
    return 1;
}
/* 1 equal, 0 different, -1 I/O/type error. No stale errno for differences. */
static int restore_matches(const char *path,const RestoreInput *in) {
    struct stat st;
    if(lstat(path,&st))return -1;
    if(!S_ISREG(st.st_mode)){errno=EINVAL;return -1;}
    if(st.st_size<0||(uint64_t)st.st_size!=in->size){errno=0;return 0;}
    FILE *f=fopen(path,"rb");if(!f)return -1;
    unsigned char *a=malloc(RESTORE_CHUNK*2u);int result=1;
    if(!a){fclose(f);errno=ENOMEM;return -1;}
    unsigned char *b=a+RESTORE_CHUNK;
    for(uint64_t off=0;off<in->size;) {
        size_t n=in->size-off>RESTORE_CHUNK?RESTORE_CHUNK:(size_t)(in->size-off);
        errno=0;
        if(!restore_read(in,off,a,n)||fread(b,1,n,f)!=n){if(!errno)errno=EIO;result=-1;break;}
        if(memcmp(a,b,n)){errno=0;result=0;break;}off+=n;
    }
    int saved=errno;free(a);fclose(f);errno=saved;return result;
}
/* Parse JSON incrementally; file size is not an allocation size. */
static struct json_object *restore_json(const RestoreInput *in) {
    struct json_tokener *tok=json_tokener_new();if(!tok){errno=ENOMEM;return NULL;}
    json_tokener_set_flags(tok,JSON_TOKENER_STRICT);
    unsigned char *buf=malloc(RESTORE_CHUNK);
    if(!buf){json_tokener_free(tok);errno=ENOMEM;return NULL;}
    struct json_object *obj=NULL;int good=1,complete=0;
    for(uint64_t off=0;off<in->size&&good;) {
        size_t n=in->size-off>RESTORE_CHUNK?RESTORE_CHUNK:(size_t)(in->size-off);
        if(!restore_read(in,off,buf,n)){good=0;break;}
        size_t start=off==0&&n>=3&&!memcmp(buf,"\xef\xbb\xbf",3)?3:0;
        if(!complete) {
            obj=json_tokener_parse_ex(tok,(char *)buf+start,(int)(n-start));
            enum json_tokener_error err=json_tokener_get_error(tok);
            if(err==json_tokener_success) {
                complete=1;start+=json_tokener_get_parse_end(tok);
            }else if(err==json_tokener_continue){off+=n;continue;}
            else {
                fprintf(logfile,"JSON_PARSE_FAILED offset=%llu reason=%s\n",
                        (unsigned long long)(off+start+json_tokener_get_parse_end(tok)),json_tokener_error_desc(err));
                errno=EINVAL;good=0;break;
            }
        }
        for(size_t j=start;j<n;j++)if(!isspace(buf[j])){errno=EINVAL;good=0;break;}
        off+=n;
    }
    free(buf);json_tokener_free(tok);
    if(!good||!complete||!obj||!json_object_is_type(obj,json_type_object)) {
        if(obj)json_object_put(obj);if(good)errno=EINVAL;return NULL;
    }
    return obj;
}
static int restore_string_equal(struct json_object *a,struct json_object *b,const char *key) {
    struct json_object *x=NULL,*y=NULL;
    return json_object_object_get_ex(a,key,&x)&&json_object_object_get_ex(b,key,&y)&&
        json_object_is_type(x,json_type_string)&&json_object_is_type(y,json_type_string)&&
        json_object_get_string_len(x)>0&&json_object_get_string_len(x)==json_object_get_string_len(y)&&
        !memcmp(json_object_get_string(x),json_object_get_string(y),(size_t)json_object_get_string_len(x));
}
static int restore_title_matches(struct json_object *obj,const char *title) {
    struct json_object *id=NULL;
    return obj&&json_object_object_get_ex(obj,"titleId",&id)&&json_object_is_type(id,json_type_string)&&
        json_object_get_string_len(id)==(int)strlen(title)&&!strcmp(json_object_get_string(id),title);
}
static int restore_identity_matches(struct json_object *a,struct json_object *b) {
    return restore_string_equal(a,b,"titleId")&&restore_string_equal(a,b,"contentId")&&
        restore_string_equal(a,b,"contentVersion");
}
static int restore_write_exclusive(const char *path,const RestoreInput *in) {
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0644);if(fd<0)return 0;
    unsigned char *buf=malloc(RESTORE_CHUNK);int good=1;
    if(!buf){close(fd);unlink(path);errno=ENOMEM;return 0;}
    for(uint64_t off=0;off<in->size&&good;) {
        size_t n=in->size-off>RESTORE_CHUNK?RESTORE_CHUNK:(size_t)(in->size-off);
        if(!restore_read(in,off,buf,n)){good=0;break;}
        size_t pos=0;
        while(pos<n) {
            ssize_t k=write(fd,buf+pos,n-pos);
            if(k<0&&errno==EINTR)continue;
            if(k<=0){if(!k)errno=EIO;good=0;break;}pos+=(size_t)k;
        }
        off+=n;
    }
    free(buf);
    if(good&&fsync(fd))good=0;
    int saved=errno;if(close(fd)&&good){good=0;saved=errno;}errno=saved;
    if(good&&restore_matches(path,in)!=1){good=0;if(!errno)errno=EIO;}
    if(!good){saved=errno;unlink(path);errno=saved;}
    return good;
}
static int restore_temp(const char *path,const RestoreInput *in,char *temp,size_t cap) {
    for(unsigned tries=0;tries<100;tries++) {
        int n=snprintf(temp,cap,"%s.restore-1.3-%d-%u.tmp",path,getpid(),++temp_serial);
        if(n<0||(size_t)n>=cap){errno=ENAMETOOLONG;return 0;}
        if(restore_write_exclusive(temp,in))return 1;
        if(errno!=EEXIST)return 0;
    }
    errno=EEXIST;return 0;
}
static int restore_backup(const char *path,const char *title,const char *slot,
                          const RestoreInput *old,char *backup,size_t cap) {
    if(!directory(STATE "/backups"))return 0;
    for(unsigned tries=0;tries<1000;tries++) {
        int n=snprintf(backup,cap,STATE "/backups/%s-%s-%d-%u.param.json",title,slot,getpid(),++backup_serial);
        if(n<0||(size_t)n>=cap){errno=ENAMETOOLONG;return 0;}
        if(restore_write_exclusive(backup,old)) {
            fprintf(logfile,"BACKUP original=%s saved=%s bytes=%llu\n",path,backup,(unsigned long long)old->size);
            if(fflush(logfile)||fsync(fileno(logfile)))return 0;
            return 1;
        }
        if(errno!=EEXIST)return 0;
    }
    errno=EEXIST;return 0;
}
/* PKG is authoritative only for the same title/content/version. */
static int restore_stage(const char *path,const RestoreInput *in,int write_it,int is_param,
                         struct json_object *expected,const char *title,const char *slot) {
    struct stat st;int exists=!lstat(path,&st);
    if(!exists&&errno!=ENOENT)return restore_fail(path,"stat",errno);
    if(exists&&!S_ISREG(st.st_mode))return restore_fail(path,"not-regular-file",EINVAL);
    if(exists&&!is_param)return 1;
    FILE *old_file=NULL;RestoreInput old={0};int changed=0,good=0;char temp[640]={0},backup[640]={0};
    if(exists) {
        int match=restore_matches(path,in);
        if(match==1)return 1;
        if(match<0)return restore_fail(path,"compare",errno);
        old_file=fopen(path,"rb");if(!old_file)return restore_fail(path,"open-existing-param",errno);
        old=(RestoreInput){old_file,0,(uint64_t)st.st_size};
        struct json_object *obj=restore_json(&old);
        int same=obj&&restore_identity_matches(obj,expected);
        if(obj)json_object_put(obj);
        if(!same){restore_fail(path,"param-title-content-version-mismatch-or-invalid",EINVAL);goto done;}
        changed=1;
    }
    if(!write_it){good=1;goto done;}
    if(!restore_temp(path,in,temp,sizeof(temp))){restore_fail(path,"write-temp",errno);goto done;}
    if(changed) {
        if(!restore_backup(path,title,slot,&old,backup,sizeof(backup))){restore_fail(path,"backup",errno);goto done;}
        /* Reopen the immutable backup and compare against the live destination. */
        FILE *saved=fopen(backup,"rb");
        if(!saved){restore_fail(path,"open-backup",errno);goto done;}
        RestoreInput snapshot={saved,0,old.size};int match=restore_matches(path,&snapshot);
        fclose(saved);
        if(match!=1){restore_fail(path,"changed-during-restore",EBUSY);goto done;}
        fclose(old_file);old_file=NULL;
    }else if(!lstat(path,&st)||errno!=ENOENT) {
        restore_fail(path,"destination-appeared",EEXIST);goto done;
    }
    if(rename(temp,path)){restore_fail(path,"rename",errno);goto done;}
    temp[0]=0;
    if(restore_matches(path,in)!=1){restore_fail(path,"post-write-verify",EIO);goto done;}
    if(changed)updated++;else created++;
    fprintf(logfile,"%s %s bytes=%llu%s%s\n",changed?"UPDATED":"CREATED",path,
            (unsigned long long)in->size,changed?" backup=":"",changed?backup:"");
    good=1;
done:
    if(old_file)fclose(old_file);
    if(temp[0])unlink(temp);
    return good;
}
#endif
