#ifndef _FAT16_H
#define _FAT16_H

/* FAT基本信息 */
#define FAT_TYPE 16             /* FAT16 */
#define FAT_BYTE 2              /* 每个表项占2字节 */

typedef struct _FAT_TIME FAT_TIME, *PFAT_TIME; /* 时间 */
typedef struct _FAT_DATE FAT_DATE, *PFAT_DATE; /* 日期 */
typedef struct _DIRENT DIRENT, *PDIRENT;       /* 目录项 */

/* 时间\日期结构体 */
struct _FAT_TIME {
    unsigned short DoubleSeconds:5;
    unsigned short Minute:6;
    unsigned short Hour:5;
};

struct _FAT_DATE {
    unsigned short Day:5;
    unsigned short Month:4;
    unsigned short Year:7;      /* 1980起 */
};

/* 目录项结构体，32字节 */
struct _DIRENT {
    char Name[8];              /* 文件名 */
    char Extend[3];            /* 扩展名 */
    unsigned char Attributes;   /* 文件属性 */
    unsigned char Reserved[10]; /* 保留未用 */
    FAT_TIME LastWriteTime;     /* 最后修改时间 */
    FAT_DATE LastWriteDate;     /* 最后修改日期 */
    unsigned short FirstCluster; /* 初始块号 */
    unsigned long FileSize;      /* 文件大小 */
};


/* 文件属性的位定义 */
#define DIRENT_ATTR_READ_ONLY		0x01 /* 只读 */
#define DIRENT_ATTR_HIDDEN			0x02 /* 隐藏 */
#define DIRENT_ATTR_SYSTEM			0x04 /* 系统 */
#define DIRENT_ATTR_DIRECTORY		0x10 /* 目录 */

/* 各区空间分配 */
#define BOOT_SECTOR 0           /* 引导区扇区号 */
#define FAT_SECTOR_START 1      /* fat表初始扇区号 */
#define FAT_SECTOR_SIZE 18      /* fat表占用扇区数 */
#define FAT_DIRENT_START 19     /* 目录项初始扇区号 */
#define FAT_DIRENT_SIZE 14      /* 目录项占用扇区数 */
#define FAT_DATA_START 33       /* 数据区起始扇区数 */
#define FAT_SIZE 2017      /* 数据区总簇数 */


/* fat表内数值 */
#define FAT_SPACE_UNUSED 0x0000 /* 未被分配 */
#define FAT_SPACE_BAD 0xFFF7    /* 坏簇 */
#define FAT_SPACE_END 0xFFFF    /* 文件结束簇 */
#define FAT_CLUSTER_0 0xFF0F    /* 表项0的信息 */
#define FAT_CLUSTER_1 0xFFFF    /* 表项1的信息 */

/* 目录项字节数 */
#define DIRENT_SIZE 32

/* fat操作 */
int FatWriteItem(int item, unsigned short info);
unsigned short FatReadItem(int item);
int FatFree(int item);
void FatClean();
void FatInit();
int FatFindSpace();

/* 目录项 */
int DirentWrite(int cluster, int item, PDIRENT dir);
int DirentRead(int cluster, int item, PDIRENT dir);
void DirentFree(int cluster, int item);
int DirentPrint(PDIRENT dir);
int DirentLength(int cluster);
int DirentFindSpc(int cluster);
int DirentFindBy(int cluster, char *dirName);
void DirentSet(PDIRENT dir, char *n, unsigned char a,
               unsigned short c, unsigned long s);

/* 时间 */
FAT_TIME FatGetCurTime();
FAT_DATE FatGetCurDate();


#endif  /* _FAT16_H */
