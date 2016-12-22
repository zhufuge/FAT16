#ifndef _CMD_H
#define _CMD_H
#include "fat16.h"

typedef struct _FCB FCB, *PFCB;

/* 文件控制块，与目录项结构体对应 */
struct _FCB {
    char Name[13];              /* 文件名 */

    /* 文件属性 */
    int AttrReadOnly;          /* 只读 */
    int AttrHidden;            /* 隐藏 */
    int AttrSystem;            /* 系统 */
    int AttrDirectory;         /* 目录 */

    /* 时间和日期 */
    FAT_TIME LastWriteTime;     /* 最后修改时间 */
    FAT_DATE LastWriteDate;     /* 最后修改日期 */

    /* 初始块号、文件大小 */
    unsigned short FirstCluster; /* 初始簇号 */
    unsigned long FileSize;      /* 文件大小 */

    /* 目录项结构体在目录文件内的偏移 */
    unsigned long DirEntryOffset;

    /* 文件所在目录的指针，如果为 NULL 说明文件位于根目录。 */
    PFCB ParentDirectory;
};

/* FCB */
void FCBCreate(PFCB fcb);
void FCBFree(PFCB fcb);

/* 状态 */
void Fat_st(PFCB fcb);          /* 磁盘状态 */

/* 目录操作 */
void Fat_cd(PFCB *fcb, char *dirName);   /* 更改当前目录 */
void Fat_mkdir(PFCB fcb, char *dirName); /* 创建子目录 */
void Fat_rmdir(PFCB fcb, char *dirName); /* 删除子目录 */
void Fat_ls(PFCB fcb);                   /* 显示目录 */

/* 文件操作 */
void Fat_create(PFCB fcb, char *fileName);              /* 创建 */
void Fat_rm(PFCB fcb, char *fileName);                  /* 删除 */
void Fat_write(PFCB fcb, char *fileName);               /* 写 */
void Fat_read(PFCB fcb, char *fileName);                /* 读 */

/* 目录，文件改名 */
void Fat_rename(PFCB fcb, char *fileName, char *NewName);

#endif  /* _CMD_H */
