#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fat16.h"
#include "cmd.h"

#define FILE_BUFFER_MAX_SIZE 512

static void DIR2FCB(PFCB fcb, PDIRENT dir, unsigned long offset, PFCB par)
{
    /* 设置FCB的值 */
    strncpy(fcb->Name, dir->Name, 8);

    fcb->AttrReadOnly = (*dir->Attributes & DIRENT_ATTR_READ_ONLY) ? 1 : 0;
    fcb->AttrHidden = (*dir->Attributes & DIRENT_ATTR_HIDDEN) ? 1 : 0;
    fcb->AttrSystem = (*dir->Attributes & DIRENT_ATTR_SYSTEM) ? 1 : 0;
    fcb->AttrDirectory = isDir(dir);

    fcb->LastWriteTime = dir->LastWriteTime;
    fcb->LastWriteDate = dir->LastWriteDate;

    fcb->FirstCluster =  dir->FirstCluster;
    fcb->FileSize = dir->FileSize;

    fcb->DirEntryOffset = offset;
    fcb->ParentDirectory = par;
}

static void fileRemove(PDIRENT dir)
{
    /* 只删除文件内容， 和fat表项 */
    unsigned short item = dir->FirstCluster;
    if (item == FAT_SPACE_UNUSED) {
        return ;
    }

    unsigned short tmp;
    /* 顺着fat表将文件占有的簇块全部清空 */
    while (item != FAT_SPACE_END) {
        ClusterClean(item);
        tmp = item;
        item = FatReadItem(item);
        FatFree(tmp);
    }
}

static void dirRemove(PDIRENT dir)
{
    /* 递归删除目录，内容及目录项所有内容 */
    int cluster = dir->FirstCluster;
    PDIRENT child = malloc(sizeof(struct _DIRENT));

    DirentFree(cluster, 0);
    DirentFree(cluster, 1);

    int i;
    for (i = 2; i < DirentLength(cluster); i++) {
        if (DirentRead(cluster, i, child) == 0) {
            if (!isDir(child)) {
                /* 先删除目录下所有的文件 */
                fileRemove(child);
                DirentFree(cluster, i);
            } else {
                /* 再删除目录下的所有子目录 */
                dirRemove(child);
                DirentFree(cluster, i);
            }
        }
    }
    FatFree(cluster);
}

static int Fat_open(int cluster, char *fileName, PDIRENT dir)
{
    /* 处理文件名 */
    char name[8];
    char *token = strtok(fileName, ".");
    strncpy(name, token, 8);

    /* 寻找该文件 */
    int found = DirentFindBy(cluster, name);
    if (found == -1) {
        printf("file operate failed: <%s> not found!\n", fileName);
        return found;
    }

    /* 判断是否是文件 */
    DirentRead(cluster, found, dir);
    if (isDir(dir)) {
        printf("file operate failed: <%s> is a dir!\n", fileName);
        return -1;
    }

    return found;
}

static void fileWriteOperate(int endOfFile, int fileCluster, int *fileSize)
{
    /* 写操作 */
    int sizeOfFile = *fileSize;

    char line[100];
    while (1) {
        /* 处理输入 */
        gets(line);
        if (strcmp(line, "cls()") == 0) {
            break;
        }
        /* 添加回车 */
        strncat(line, "\r\n", 2);

        int size = (int) strlen(line);
        int out = isOutOfSector(endOfFile + size);
        if (out >= 0) {
            /* 写时，跨簇，则分配新簇，供写 */
            /* 先将未跨簇的字符写入 */
            FileWriteLine(fileCluster, endOfFile, line, size + out);

            /* 分配空闲磁盘块 */
            int space = FatFindSpace();
            if (space == -1) {
                printf("mrdir failed: out of space!\n");
                return ;
            }
            /* 改写FAT表项 */
            FatWriteItem(fileCluster, space);
            fileCluster = space;
            FatWriteItem(space, FAT_SPACE_END);

            /* 将剩下的字符写入新簇 */
            char *sur = &line[size - out];
            FileWriteLine(fileCluster, 0, sur, out);
            endOfFile = out;
        } else {
            FileWriteLine(fileCluster, endOfFile, line, size);
            endOfFile += size;
        }
        sizeOfFile += size;
        *fileSize = sizeOfFile;
    }
}

void FCBCreate(PFCB fcb)
{
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    DirentSet(dir, "~", DIRENT_ATTR_DIRECTORY, 1, 0);
    DIR2FCB(fcb, dir, -1, NULL);
    free(dir);
}

void FCBFree(PFCB fcb)
{
    /* 释放FCB链 */
    PFCB tmp = fcb->ParentDirectory;
    free(fcb);
    while (tmp != NULL) {
        fcb = tmp;
        tmp = fcb->ParentDirectory;
        free(fcb);
    }
}

void Fat_ls(PFCB fcb)
{
    /* 显示当前目录 */

    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    int dirCount = 0;           /* 目录数 */
    int fileCount = 0;          /* 文件数 */

    int cluster = fcb->FirstCluster;

    int i;
    for (i = 0; i < DirentLength(cluster); i++) {
        if (DirentRead(cluster, i, dir) == 0) {
            if (DirentPrint(dir)) {
                fileCount++;
            } else {
                dirCount++;
            }
        }
    }

    printf("\t\t%d dirs\n", dirCount);
    printf("\t\t%d files\n", fileCount);
    free(dir);
}

void Fat_mkdir(PFCB fcb, char *dirName)
{
    /* 创建子目录 */

    /* 处理目录名 */
    char name[8];
    strncpy(name, dirName, 8);

    int cluster = fcb->FirstCluster;

    /* 判断是否有同名目录 */
    if (DirentFindBy(cluster, name) != -1) {
        printf("mrdir failed: dir<%s> was exist!\n", name);
        return ;
    }

    /* 找到第一个空闲目录项 */
    int dpos = DirentFindSpc(cluster);
    if (dpos == -1) {
        printf("mrdir failed: dirent was fulled!\n");
        return ;
    }

    /* 找空闲磁盘块 */
    int space = FatFindSpace();
    if (space == -1) {
        printf("mrdir failed: out of space!\n");
        return ;
    }

    /* 使用磁盘块 */
    FatWriteItem(space, FAT_SPACE_END);

    /* 填写目录项信息 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    DirentSet(dir, name, DIRENT_ATTR_DIRECTORY, space, 0);

    /* 写入目录项 */
    DirentWrite(cluster, dpos, dir);

    /* 将块作为目录项表，并在块内写入父目录和自身目录 */
    DirentSet(dir, ".", DIRENT_ATTR_DIRECTORY, space, 0);
    DirentWrite(space, 0, dir);

    DirentSet(dir, "..", DIRENT_ATTR_DIRECTORY, cluster, 0);
    DirentWrite(space, 1, dir);

    free(dir);
}

void Fat_rmdir(PFCB fcb, char *dirName)
{
    /* 删除子目录,及目录下所有内容 */

    if (strcmp(dirName, ".") == 0 &&
        strcmp(dirName, "..") == 0) {
        printf("rmdir failed: %s coundn't remove!\n", dirName);
        return ;
    }

    char name[8];
    strncpy(name, dirName, 8);

    int cluster = fcb->FirstCluster;

    /* 判断是有该目录 */
    int found = -1;
    found = DirentFindBy(cluster, name);
    if (found == -1) {
        printf("Not Found dir<%s>\n", name);
        return ;
    }

    /* 找到则删除, 并删除子项 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    DirentRead(cluster, found, dir);
    dirRemove(dir);
    DirentFree(cluster, found);

    free(dir);
}

void Fat_cd(PFCB *fcb, char *dirName)
{
    /* 切换目录 */

    if (strcmp(dirName, ".") == 0) {
        return ;
    } else if (strcmp(dirName, "..") == 0) {
        if ((*fcb)->ParentDirectory == NULL) {
            return ;
        }
        PFCB tmp = *fcb;
        *fcb = (*fcb)->ParentDirectory;
        free(tmp);
        return ;
    }

    char name[8];               /* 处理文件名 */
    strncpy(name, dirName, 8);

    int cluster = (*fcb)->FirstCluster;

    /* 判断是否有同名目录 */
    int offset = DirentFindBy(cluster, name);
    if (offset == -1) {
        printf("cd failed: dir<%s> not found!\n", name);
        return ;
    }

    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    DirentRead(cluster, offset, dir);

    PFCB child = malloc(sizeof(struct _FCB));
    DIR2FCB(child, dir, offset, (*fcb));
    sprintf(child->Name, "%s/%s", (*fcb)->Name, name);
    *fcb = child;

    free(dir);
}

void Fat_create(PFCB fcb, char *fileName)
{
    /* 创建文件 */
    /* 处理文件名 */
    char name[8];
    char extend[3];
    char *token;

    int cluster = fcb->FirstCluster;

    token = strtok(fileName, ".");
    strncpy(name, token, 8);
    token = strtok(NULL, ".");
    if (!token) {
        printf("create failed: extend is null!\n");
        return ;
    }

    /* 判断是否有同名文件 */
    if (DirentFindBy(cluster, name) != -1) {
        printf("create failed: <%s> was exist!\n", name);
        return ;
    }

    /* 找到第一个空闲目录项 */
    int dpos = DirentFindSpc(cluster);
    if (dpos == -1) {
        printf("create failed: dirent was fulled!\n");
        return ;
    }

    /* 填写目录项信息 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    DirentSet(dir, name, 0, FAT_SPACE_UNUSED, 0);
    strncpy(extend, token, 3);
    strncpy(dir->Extend, extend, 3);

    /* 写入目录项 */
    DirentWrite(cluster, dpos, dir);

    free(dir);
}

void Fat_write(PFCB fcb, char *fileName)
{
    /* 写文件 */

    int cluster = fcb->FirstCluster;

    /* 找文件 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    int found = Fat_open(cluster, fileName, dir);
    if (found == -1) {
        free(dir);
        return ;
    }

    int sizeOfFile = 0;         /* 文件大小 */
    int endOfFile = 0;          /* 记录文件末尾 */
    int fileCluster;            /* 文件簇号 */
    if (isEmptyFile(dir)) {
        /* 判断是否为空文件，空则分配簇区 */
        int space = FatFindSpace();                  /* 空闲磁盘块 */
        if (space == -1) {
            printf("mrdir failed: out of space!\n");
            free(dir);
            return ;
        }

        dir->FirstCluster = space;
        FatWriteItem(space, FAT_SPACE_END);
        fileCluster = space;
    } else {
        /* 非空，则通过文件大小，判断末尾在第几个簇区 */
        sizeOfFile = dir->FileSize;
        endOfFile = inClusterPos(sizeOfFile);
        fileCluster = nextNCluster(dir->FirstCluster, inClusterIndex(sizeOfFile));
    }

    /* 文件末尾开始写内容 */
    printf("%s : write >\n", fileName);
    fileWriteOperate(endOfFile, fileCluster, &sizeOfFile);

    /* 将信息写回目录项 */
    dir->LastWriteTime = FatGetCurTime();
    dir->LastWriteDate = FatGetCurDate();
    dir->FileSize = sizeOfFile;

    DirentWrite(cluster, found, dir);
    free(dir);
}

void Fat_read(PFCB fcb, char *fileName)
{
    /* 读文件 */
    /* 还有问题 */
    printf("rename cmd have a bug!\n");
    return ;

    int cluster = fcb->FirstCluster;

    /* 找文件 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    int found = Fat_open(cluster, fileName, dir);
    if (found == -1) {
        free(dir);
        return ;
    }

    /* 开始读 */
    printf("%s : read >\n", fileName);
    int sizeOfFile = dir->FileSize;
    if (sizeOfFile == 0) {
        free(dir);
        return ;
    }

    int fileCluster = dir->FirstCluster;
    char content[FILE_BUFFER_MAX_SIZE] = "";

    /* TODO:有问题 */
    FileReadCluster(fileCluster, content);
    printf("%s", content);

    free(dir);
}

void Fat_rm(PFCB fcb, char *fileName)
{
    /* 删除文件 */

    int cluster = fcb->FirstCluster;

    /* 判断是否有该文件 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    int found = Fat_open(cluster, fileName, dir);
    if (found == -1) {
        free(dir);
        return ;
    }

    /* 有，则清空占有簇块和fat表 */
    fileRemove(dir);

    /* 清空该目录项 */
    DirentFree(cluster, found);

    free(dir);
}

void Fat_mv(PFCB fcb, char *fileName, char *dirName)
{
    /* 将文件移到上层某个同层目录下 */
}

void Fat_rename(PFCB fcb, char *fileName, char *NewName)
{
    /* 重命名文件或目录 */
    /* 还有问题 */
    printf("rename cmd have a bug!\n");
    return ;

    int cluster = fcb->FirstCluster;

    char name[8];
    strncpy(name, NewName, 8);

    /* 找到该文件或目录 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    int found = DirentFindBy(cluster, fileName);
    if (found == -1) {
        printf("rename failed: <%s> not found!\n", fileName);
        free(dir);
        return ;
    }

    /* 判断是否新文件名与其他文件同名 */
    if (DirentFindBy(cluster, name) != -1) {
        printf("rename failed: <%s> was exist!\n", name);
        free(dir);
        return ;
    }

    /* 改名 */
    DirentSet(dir, NewName, *dir->Attributes, dir->FirstCluster, dir->FileSize);

    /* 抹去重写 */
    DirentFree(cluster, found);
    DirentWrite(cluster, found, dir);

    free(dir);
}

void Fat_st(PFCB fcb)
{
    /* 查看磁盘的状态 */
    /* 只能在根目录下使用 */

    /* 判断当前目录是否为根目录 */
    if (fcb->ParentDirectory == NULL) {
        /* 打印磁盘空间使用信息 */
        printf("status:\n");
        FatStatus();
    } else {
        printf("st failed: current dir isn't boot dir!\n");
    }

}
