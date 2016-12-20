#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "io.h"
#include "fat16.h"
#include "api.h"

static void FCBCreate(PFCB fcb, PDIRENT dir, unsigned long offset, PFCB par)
{
    /* 设置FCB的值 */
    strncpy(fcb->Name, dir->Name, 8);

    fcb->AttrReadOnly = (dir->Attributes & DIRENT_ATTR_READ_ONLY) ? 1 : 0;
    fcb->AttrHidden = (dir->Attributes & DIRENT_ATTR_HIDDEN) ? 1 : 0;
    fcb->AttrSystem = (dir->Attributes & DIRENT_ATTR_SYSTEM) ? 1 : 0;
    fcb->AttrDirectory = (dir->Attributes & DIRENT_ATTR_DIRECTORY) ? 1 : 0;

    fcb->LastWriteTime = dir->LastWriteTime;
    fcb->LastWriteDate = dir->LastWriteDate;

    fcb->FirstCluster =  dir->FirstCluster;
    fcb->FileSize = dir->FileSize;

    fcb->DirEntryOffset = offset;
    fcb->ParentDirectory = par;
}

static void FCBFree(PFCB fcb)
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

/* 系统 */
void FatStart()
{
    /* 进入文件系统 */

    FatInit();

    printf("Welcome to FAT16 system!\n");
    char cmd[20];               /* 接收输入字符串 */
    char *token;                /* 用于分割字符串 */

    PFCB cur = malloc(sizeof(struct _FCB));
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    DirentSet(dir, "~", DIRENT_ATTR_DIRECTORY, 1, 0);
    FCBCreate(cur, dir, -1, NULL);

    char cmdList[][8] = {
        "ls",
        "exit",
        "cd",
        "mkdir",
        "rmdir",
        "create"
    };

    int cmdSize = 6;

    int flag = 1;
    while (flag) {
        printf("%s > ", cur->Name);

        gets(cmd);
        token = strtok(cmd, " ");

        int i;
        for (i = 0; i < cmdSize; i++) {
            if (strcmp(token, cmdList[i]) == 0) {
                break;
            }
        }

        token = strtok(NULL, " ");

        if (token) {
            switch (i) {
            case 2:
                Fat_cd(&cur, token);
                break;
            case 3:
                Fat_mkdir(cur, token);
                break;
            case 4:
                Fat_rmdir(cur, token);
            case 5:
                Fat_create(cur, token);
            default:
                break;
            }
        } else {
            switch (i) {
            case 0:
                Fat_ls(cur);
                break;
            case 1:
                flag = 0;
                break;
            default:
                printf("Wrong command!\n");
                break;
            }
        }
    }

    FCBFree(cur);
    free(dir);
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
    /* 删除子目录 */

    char name[8];
    strncpy(name, dirName, 8);

    int cluster = fcb->FirstCluster;

    /* 判断是有该目录 */
    int found = -1;
    found = DirentFindBy(cluster, name);

    if (found == -1) {
        printf("Not Found dir<%s>\n", name);
    } else {
        /* 找到则删除, 并删除子项 */
        /* DirentRemove(); */
        /* FileRemove(); */
    }
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
    FCBCreate(child, dir, offset, (*fcb));
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

    token = strtok(fileName, ".");
    strncpy(name, token, 8);
    token = strtok(NULL, ".");
    strncpy(extend, token, 3);

    int cluster = fcb->FirstCluster;

    /* 判断是否有同名文件 */
    if (DirentFindBy(cluster, name) != -1) {
        printf("create failed: <%s> was exist!\n", name);
        return ;
    }

    /* 找到第一个空闲目录项 */
    int dpos = DirentFindSpc(cluster);
    if (dpos == -1) {
        printf("mrdir failed: dirent was fulled!\n");
        return ;
    }

    /* 填写目录项信息 */
    PDIRENT dir = malloc(sizeof(struct _DIRENT));
    DirentSet(dir, name, 0, FAT_SPACE_UNUSED, 0);
    strncpy(dir->Extend, extend, 3);

    /* 写入目录项 */
    DirentWrite(cluster, dpos, dir);

    free(dir);
}

