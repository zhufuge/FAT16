#ifndef _IO_H
#define _IO_H

#define FAT_STORAGE_NAME "FATFOS"      /* 磁盘名 */
#define FAT_STORAGE_SIZE (1024 * 1024) /* 磁盘大小 */

/* 扇区大小定义 */
#define SECTOR_SIZE 512

/* 扇区读写 */
int IOReadSector(int sector, int pos, void *buffer, int size, int count);
int IOWriteSector(int sector, int pos, void *buffer, int size, int count);

#endif  /* _IO_H */
