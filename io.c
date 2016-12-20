#include <stdio.h>
#include <stdlib.h>
#include "io.h"

/* 扇区所在位置 */
#define SECTOR_POS(s) (s * SECTOR_SIZE)


static FILE *FatCreateStorage()
{
    /*
      以文件形式，创建磁盘空间
    */
    FILE *file = fopen(FAT_STORAGE_NAME, "wb+");
    if (!file) {
        perror("Storage opening failed!");
        exit(EXIT_FAILURE);
    } else {
        /* 分配固定大小 */
        fseek(file, FAT_STORAGE_SIZE - 1, SEEK_SET);
        fputc(32, file);
    }

    return file;
}

static FILE *FatInitStorage()
{
    /*
      初始化磁盘空间
      未分配，则分配，并返回
      已分配，则直接返回
    */
    FILE *fp = fopen(FAT_STORAGE_NAME, "r+");
    if (!fp) {
        fp = FatCreateStorage();
        perror("Create a storage.\n");
    }

    return fp;
}

static FILE *FatOpenStorage()
{
    /*
      打开磁盘文件
    */
    return FatInitStorage();
}

static int FatCloseStorage(FILE *fp)
{
    /*
      关闭磁盘文件
    */
    return fclose(fp);
}

static int IOReadStroage(int pos, void *buffer, int size, int count)
{
    /*
      利用文件读函数，模拟磁盘读
    */
    FILE *fp = FatOpenStorage();

    /* 设置读写指针 */
    fseek(fp, pos, SEEK_SET);
    /* 读数据到 指针buffer 中 */
    int read = fread(buffer, size, count, fp);

    FatCloseStorage(fp);
    return read;
}

static int IOWriteStroage(int pos, void *buffer, int size, int count)
{
    /*
      利用文件写函数，模拟磁盘写
    */
    FILE *fp = FatOpenStorage();

    /* 设置读写指针 */
    fseek(fp, pos, SEEK_SET);
    /* 将 指针buffer 中数据写入 */
    int write = fwrite(buffer, size, count, fp);

    FatCloseStorage(fp);
    return write;
}

int IOReadSector(int sector, int pos, void *buffer, int size, int count)
{
    /* 扇区读 */
    return IOReadStroage(SECTOR_POS(sector) + pos, buffer, size, count);
}

int IOWriteSector(int sector, int pos, void *buffer, int size, int count)
{
    /* 扇区写 */
    return IOWriteStroage(SECTOR_POS(sector) + pos, buffer, size, count);
}
