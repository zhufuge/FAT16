#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fatsys.h"
#include "cmd.h"


/* 系统 */
void FatStart()
{
    /* 进入文件系统 */

    FatInit();

    printf("Welcome to FAT16 system!\n");
    char cmd[32];               /* 接收输入字符串 */
    char *token;                /* 用于分割字符串 */

    PFCB cur = malloc(sizeof(struct _FCB));
    FCBCreate(cur);

    char cmdList[][8] = {
        "ls",
        "exit",
        "cd",
        "mkdir",
        "rmdir",
        "create",
        "write",
        "read",
        "rm",
        "st",
        "rename"
    };

    int cmdSize = 11;

    int flag = 1;
    while (flag) {
        printf("%s > ", cur->Name);

        gets(cmd);
        if (strcmp(cmd, "") == 0) {
            continue;
        }
        token = strtok(cmd, " ");

        int i;
        for (i = 0; i < cmdSize; i++) {
            if (strcmp(token, cmdList[i]) == 0) {
                break;
            }
        }

        if (i == cmdSize) {
            printf("Wrong command!\n");
            continue;
        }

        token = strtok(NULL, " ");

        if (token) {
            /* 有参数的命令 */
            switch (i) {
            case 2:
                Fat_cd(&cur, token);
                break;
            case 3:
                Fat_mkdir(cur, token);
                break;
            case 4:
                Fat_rmdir(cur, token);
                break;
            case 5:
                Fat_create(cur, token);
                break;
            case 6:
                Fat_write(cur, token);
                break;
            case 7:
                Fat_read(cur, token);
                break;
            case 8:
                Fat_rm(cur, token);
                break;
            default:
                break;
            }

            char tmp[11];
            strncpy(tmp, token, 11);
            token = strtok(NULL, " ");
            if (token) {
                switch (i) {
                case 10:
                    Fat_rename(cur, tmp, token);
                    break;
                default:
                    break;
                }
            }

        } else {
            switch (i) {
            case 0:
                Fat_ls(cur);
                break;
            case 1:
                flag = 0;
                break;
            case 9:
                Fat_st(cur);
                break;
            default:
                break;
            }
        }
    }

    FCBFree(cur);
}

