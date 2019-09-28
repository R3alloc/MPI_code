#include "mpi.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>

#define BUFSIZE (256*1024)
#define CMDSIZE 80
#define MAXPATHLEN 100
 
using namespace std;
int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    for(int i = 0; i < argc; i++)
    {
        printf("%s\n", argv[i]);
    }
    
    MPI_Group worldGrp;
    MPI_Comm_group(MPI_COMM_WORLD, &worldGrp);

    //父进程总数
    int parentProcNum;

    //parent进程rank
    int parentRankInWorld;
    
    //所有进程的数量
    int allProcNum;

    MPI_Comm_size(MPI_COMM_WORLD, &parentProcNum);
    MPI_Comm_rank(MPI_COMM_WORLD, &parentRankInWorld);

    MPI_Comm allComm[parentProcNum+1];

    MPI_Comm tmpInterComm[parentProcNum+1];
    for(int i  = 0; i < parentProcNum + 1; i++)
    {
        //这里统一做初始化，因为child进程中不是所有的allComm元素都被使用到，如果不做初始化就无法free
        allComm[i]=MPI_COMM_NULL;
        tmpInterComm[i] = MPI_COMM_NULL;
    }

    //复制MPI_COMM_WORLD到allComm，不能直接使用=赋值。因为=传递的是引用，最后会出现MPI_WORLD_COMM无法释放的问题。
    MPI_Comm_create(MPI_COMM_WORLD, worldGrp, &allComm[0]);
    
    char childProgram[] = "./spawnMergeLoop";
    
    //MPI_Comm_get_parent(&parentComm);
    MPI_Comm_get_parent(&tmpInterComm[0]);
    //如果当前进程为parent，则spawn子进程
    if(tmpInterComm[0] == MPI_COMM_NULL)
    {
        for(int i = 0; i < parentProcNum; i++)
        {
            char parentProcStr[20];
            sprintf(parentProcStr, "%d", parentProcNum);
            //传递给spawn的字符串数组。注意这个数组的最后一个参数一定是NULL。
            char** argv_array = new char*[2];
            argv_array[0] = parentProcStr;
            argv_array[1] = NULL;
            if(i == 0)
            {
                //MPI_Comm_spawn(childProgram, MPI_ARGV_NULL, 2, MPI_INFO_NULL, 0, allComm[0], &tmpInterComm[0], MPI_ERRCODES_IGNORE);
                MPI_Comm_spawn(childProgram, argv_array, 2, MPI_INFO_NULL, 0, allComm[0], &tmpInterComm[0], MPI_ERRCODES_IGNORE);
            }
            else
            {
                //注意这里的参数i不能写作parentRankInWorld
                //因为每个进程都要spawn同一个i
                //MPI_Comm_spawn(childProgram, MPI_ARGV_NULL, 1, MPI_INFO_NULL, i, allComm[i], &tmpInterComm[i], MPI_ERRCODES_IGNORE);
                MPI_Comm_spawn(childProgram, argv_array, 1, MPI_INFO_NULL, i, allComm[i], &tmpInterComm[i], MPI_ERRCODES_IGNORE);
            }
            
            //让parent所属的通信域优先级更高
            MPI_Intercomm_merge(tmpInterComm[i], false, &allComm[i+1]);

            int parentRankInAllComm;
            int sizeAllComm;
            MPI_Comm_rank(allComm[i+1], &parentRankInAllComm);
            MPI_Comm_size(allComm[i+1], &sizeAllComm);
            cout << "[parent " << parentRankInAllComm <<"] has spawned new process" <<endl;
            cout << "[parent " << parentRankInAllComm <<"] size of allComm:" <<sizeAllComm <<endl;
        }

    }
    //如果当前进程为child，则直接和parent进行merge通信
    else
    {
        //打印所有的参数，确认从MPI_Comm_spawn传递过来的参数从第几个开始
        for(int i =0 ; i<argc;i++)
        {
            printf("spawned argument: %s\n",argv[i]);
        }
        
        parentProcNum = atoi(argv[1]);

        //当前进程一共需要spawn多少次
        int spawningTime ;

        //tmpInterComm[0]是当前进程获得的
        MPI_Intercomm_merge(tmpInterComm[0],false, &allComm[0]);
        int rankInAllComm0;
        //这里设定父进程数量为3
        //parentProcNum = 4;
        MPI_Comm_rank(allComm[0], &rankInAllComm0);

        //下面这一段ifelse计算的是子进程参与spawn的次数
        //如果当前进程为0号父进程的第1个子进程（0号父进程有2个子进程）
        if(rankInAllComm0 == parentProcNum)// || rankInAllComm0 == parentProcNum+1)
        {
           spawningTime = 2*parentProcNum - rankInAllComm0 -1 ; 
        }
        else
        {
           spawningTime = 2*parentProcNum - rankInAllComm0  ; 
        }
        //如果当前进程不是最后一个子进程，它需要参与spawn其他的子进程
        if(spawningTime != 0)
        { 
            char parentProc[20];
            sprintf(parentProc, "%d", parentProcNum);
            char** argv_array = new char*[2];
            argv_array[0] = parentProc;
            argv_array[1] = NULL;

            for(int i = 0 ; i < spawningTime;i++)
            {
                //注意这里调用的spawn，root进程的rank一定要与parent当中的保持一致。
                //MPI_Comm_spawn(childProgram, MPI_ARGV_NULL, 1, MPI_INFO_NULL, parentProcNum-spawningTime+i, allComm[i], &tmpInterComm[i+1], MPI_ERRCODES_IGNORE);
                MPI_Comm_spawn(childProgram, argv_array, 1, MPI_INFO_NULL, parentProcNum-spawningTime+i, allComm[i], &tmpInterComm[i+1], MPI_ERRCODES_IGNORE);
                MPI_Intercomm_merge(tmpInterComm[i+1], false, &allComm[i+1]);

                int parentRankInAllComm;
                int sizeAllComm;
                MPI_Comm_rank(allComm[i+1], &parentRankInAllComm);
                MPI_Comm_size(allComm[i+1], &sizeAllComm);
                cout << "[child " << parentRankInAllComm <<"] has spawned new process" <<endl;
                cout << "[child " << parentRankInAllComm <<"] size of allComm:" <<sizeAllComm <<endl;
            }
        }
        //如果当前进程是最后一个被spawn的子进程,则不需要参与spawn新进程，只需要做一些输出即可。
        else
        {
            int parentRankInAllComm;
            int sizeAllComm;
            MPI_Comm_rank(allComm[0], &parentRankInAllComm);
            MPI_Comm_size(allComm[0], &sizeAllComm);
            cout << "[child " << parentRankInAllComm <<"] has spawned new process" <<endl;
            cout << "[child " << parentRankInAllComm <<"] size of allComm:" <<sizeAllComm <<endl;
        }

    }

    //到这里已经完成了所有进程的spawn和合并到allComm当中。


    MPI_Group_free(&worldGrp);

    //for(int i = 0; i < parentProcNum+1; i++)
    for(int i= parentProcNum;i>=0; i--)
    {
        if(tmpInterComm[i] != MPI_COMM_NULL)
        {
            MPI_Comm_free(&tmpInterComm[i]);
        }

        if(allComm[i]!=MPI_COMM_NULL)
        {
            MPI_Comm_free(&allComm[i]);
        }
    }

    MPI_Finalize();
}

