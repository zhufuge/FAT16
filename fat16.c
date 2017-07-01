#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "string.h"
#include "fat16.h"
#include "io.h"


static int ItemSectorPos(int item)
{
    /* 表项所在扇区 */
    return FAT_BYTE * item / SECTOR_SIZE + FAT_SECTOR_START;
}

static int ItemInPos(int item)
{
    /* 扇区内位置 */
    return FAT_BYTE * (item) % SECTOR_SIZE;
}

int FatWriteItem(int item, unsigned short info)
{
    /* 设置序号为 item 的表项的值 */
    return IOWriteSector(ItemSectorPos(item), ItemInPos(item),
                         &info, sizeof(unsigned short), 1);
}

unsigned short FatReadItem(int item)
{
    /* 返回序号为 item 的表项的值 */
    unsigned short info;
    IOReadSector(ItemSectorPos(item), ItemInPos(item),
                 &info, sizeof(unsigned short), 1);
    return info;
}

int FatFree(int item)
{
    /* 将 item 表项设置为空 */
    return FatWriteItem(item, FAT_SPACE_UNUSED);
}

void FatClean()
{
    /* fat数据清空 */
    FatWriteItem(0, FAT_CLUSTER_0);
    FatWriteItem(1, FAT_CLUSTER_1);

    int i;
    for (i = 2; i < FAT_SECTOR_SIZE * SECTOR_SIZE / FAT_BYTE; i++) {
        FatFree(i);
    }
}

void FatInit()
{
    /* fat初始化 */
    if (FatReadItem(0) != FAT_CLUSTER_0) {
        printf("FAT initialization.\n");
        FatWriteItem(0, FAT_CLUSTER_0);
        FatWriteItem(1, FAT_CLUSTER_1);
    }
}

int FatFindSpace()
{
    /* 返回为空的表项 */
    int i;
    for (i = 2; i < FAT_SIZE; i++) {
        if (FatReadItem(i) == FAT_SPACE_UNUSED) {
            return i;
        }
    }

    return -1;
}

void FatStatus()
{
    /* 打印磁盘空间使用信息 */

    /* 文件系统信息 */
    printf("FAT%2d SYSTEM\n", FAT_TYPE);
    /* 总空间 */
    printf("Total disk space: %10d B\n", FAT_SIZE * SECTOR_SIZE);

    int used = 0;
    int i;
    for (i = 0; i < FAT_SIZE; i++) {
        if (FatReadItem(i) != FAT_SPACE_UNUSED) {
            used++;
        }
    }

    /* 已用空间 */
    printf("Used space      : %10d B\n", used * SECTOR_SIZE);
    /* 可用空间 */
    printf("Available space : %10d B\n", (FAT_SIZE - used) * SECTOR_SIZE);
}

static int BootSectorPos(int item)
{
    /* 簇号为 1 时，所在所在扇区 */
    return (item) * DIRENT_SIZE / SECTOR_SIZE + FAT_DIRENT_START;
}

static int DirentInPos(int item)
{
    /* 区内偏移 */
    return (item) * DIRENT_SIZE % SECTOR_SIZE;
}

static int DateSectorPos(int item)
{
    /* 簇号大于 1 时，所在扇区 */
    return (item) + FAT_DATA_START - 1;
}

int DirentWrite(int cluster, int item, PDIRENT dir)
{
    /* 设置目录项的值 */
    int sector;
    if (cluster == 1) {
        sector = BootSectorPos(item);
    } else {
        sector = DateSectorPos(cluster);
    }

    return IOWriteSector(sector, DirentInPos(item),
                         dir, sizeof(struct _DIRENT), 1);
}

int DirentRead(int cluster, int item, PDIRENT dir)
{
    /* 读簇号为 cluster 的第item 个目录项。 */
    int sector;
    if (cluster == 1) {
        sector = BootSectorPos(item);
    } else {
        sector = DateSectorPos(cluster);
    }

    IOReadSector(sector, DirentInPos(item),
                 dir, sizeof(struct _DIRENT), 1);

    /* 目录项不为空，则返回 0，否则返回 1。 */
    if(strcmp(dir->Name, "") == 0) {
        return 1;
    }
    return 0;
}

void DirentFree(int cluster, int item)
{
    DIRENT dir = {};
    DirentWrite(cluster, item, &dir);
}

int DirentPrint(PDIRENT dir)
{
    /* 打印时间 */
    printf("%4d/%02d/%02d  %02d:%02d\t",
           dir->LastWriteDate.Year + 1980,
           dir->LastWriteDate.Month + 1,
           dir->LastWriteDate.Day,
           dir->LastWriteTime.Hour,
           dir->LastWriteTime.Minute);

    /* 判断是否为文件夹 */
    unsigned char attr = *dir->Attributes;
    if (attr & DIRENT_ATTR_DIRECTORY) {
        printf("<DIR>       %s\n", dir->Name);
        return 0;
    } else {
        printf("     %6ld", (unsigned long)dir->FileSize);
        printf(" %s.%.3s\n", dir->Name, dir->Extend);
        return 1;
    }
}

int DirentLength(int cluster)
{
    /* 返回簇号为 cluster 的目录项数 */
    if (cluster == 1) {
        return SECTOR_SIZE / DIRENT_SIZE * FAT_DIRENT_SIZE;
    } else {
        return SECTOR_SIZE / DIRENT_SIZE;
    }
}

int DirentFindSpc(int cluster)
{
    /* 返回 cluster 的空闲目录项 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    int i;
    int found = -1;
    for (i = 0; i < DirentLength(cluster); i++) {
        if (DirentRead(cluster, i, dir) == 1) {
            found = i;
            break;
        }
    }
    free(dir);
    return found;
}

int DirentFindBy(int cluster, char *dirName)
{
    /* 通过文件名来找文件所在位置 */
    char name[8];
    strncpy(name, dirName, 8);
    PDIRENT dir = malloc(sizeof(struct _DIRENT));

    int i;
    int found = -1;
    for (i = 0; i < DirentLength(cluster); i++) {
        if (DirentRead(cluster, i, dir) == 0) {
            if (strcmp(dir->Name, name) == 0){
                found = i;
            }
        }
    }
    free(dir);
    return found;
}

void DirentSet(PDIRENT dir, char *n, unsigned char a,
               unsigned short c, unsigned long s)
{
    /* 为目录项设置参数 */
    strncpy(dir->Name, n, 8);
    dir->LastWriteTime = FatGetCurTime();
    dir->LastWriteDate = FatGetCurDate();
    *dir->Attributes = a;
    dir->FirstCluster = c;
    dir->FileSize = s;
}

FAT_TIME FatGetCurTime()
{
    /* 获得当前时间 */

    /* 先获得系统时间 */
    time_t timer = time(NULL);
    struct tm *tmer;
    tmer = localtime(&timer);

    /* 转换 */
    FAT_TIME time = {
        .DoubleSeconds = (tmer->tm_sec / 2),
        .Minute = tmer->tm_min,
        .Hour = tmer->tm_hour
    };

    return time;
}

FAT_DATE FatGetCurDate()
{
    /* 获得当前日期 */

    /* 系统日期 */
    time_t timer = time(NULL);
    struct tm *tmer;
    tmer = localtime(&timer);

    /* 转换 */
    FAT_DATE date = {
        .Day = tmer->tm_mday,
        .Month = tmer->tm_mon,
        .Year = tmer->tm_year - 80
    };

    return date;
}

int isDir(PDIRENT dir)
{
    return (*dir->Attributes & DIRENT_ATTR_DIRECTORY) ? 1 : 0;
}

int isEmptyFile(PDIRENT dir)
{
    return (dir->FirstCluster == FAT_SPACE_UNUSED) ? 1 : 0;
}

int isOutOfSector(int size)
{
    return size - SECTOR_SIZE;
}

int nextNCluster(int first, int n)
{
    /* 递归返回下 n 个簇号 */
    if (n == 0) {
        return first;
    } else if (FatReadItem(first) == FAT_SPACE_END) {
        return first;
    } else if (FatReadItem(first) == FAT_SPACE_UNUSED) {
        return -1;
    } else {
        return nextNCluster(FatReadItem(first), n - 1);
    }
}

int inClusterPos(int pos)
{
    return pos % SECTOR_SIZE;
}

int inClusterIndex(int pos)
{
    return pos / SECTOR_SIZE;
}

int FileWriteLine(int cluster, int pos, char *line, int size)
{
    /* 将字符串作为行，写入磁盘 */
    int sector = DateSectorPos(cluster);
    return IOWriteSector(sector, pos, line, sizeof(char), size);
}

int FileReadCluster(int cluster, char *line)
{
    int sector = DateSectorPos(cluster);
    return IOReadSector(sector, 0, line, sizeof(char), SECTOR_SIZE);
}

int ClusterClean(int cluster)
{
    char a = 0x00;
    int sector = DateSectorPos(cluster);
    return IOWriteSector(sector, 0, &a, sizeof(char), SECTOR_SIZE);
}
