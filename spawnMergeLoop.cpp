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

    
    MPI_Group worldGrp=MPI_GROUP_NULL;
	//用于在循环内部迭代使用的tmpGrp
	MPI_Group tmpGrp=MPI_GROUP_NULL;

    MPI_Comm_group(MPI_COMM_WORLD, &worldGrp);

    //父进程总数
    int parentProcNum;

    //parent进程rank
    int parentRankInWorld;
    
    //所有进程的数量
    int allProcNum;

    int parentRankInAllComm;
    int sizeAllComm;

    MPI_Comm_size(MPI_COMM_WORLD, &parentProcNum);
    MPI_Comm_rank(MPI_COMM_WORLD, &parentRankInWorld);

	MPI_Comm allComm0=MPI_COMM_NULL ;

	MPI_Comm tmpInterComm0=MPI_COMM_NULL ;

    //复制MPI_COMM_WORLD到allComm0，不能直接使用=赋值。因为=传递的是引用，最后会出现MPI_WORLD_COMM无法释放的问题。
    MPI_Comm_create(MPI_COMM_WORLD, worldGrp, &allComm0);
    
    char childProgram[] = "./spawnMergeLoop";
    
    MPI_Comm_get_parent(&tmpInterComm0);

    //如果当前进程为parent，则spawn子进程
    if(tmpInterComm0 == MPI_COMM_NULL)
    {
        for(int i = 0; i < parentProcNum; i++)
        {

            char parentProcStr[20];
            sprintf(parentProcStr, "%d", parentProcNum);

            //传递给spawn的字符串数组。注意这个数组的最后一个参数一定是NULL。
            char** argv_array = new char*[2];
            argv_array[0] = parentProcStr;
            argv_array[1] = NULL;

			cout << "In loop " << i <<": before MPI_Barrier(allComm0) and Comm_spawn" << endl;	
            MPI_Comm_rank(allComm0, &parentRankInAllComm);
            MPI_Comm_size(allComm0, &sizeAllComm);
            cout << "[before spawn allComm0: parent " << parentRankInAllComm <<"] is going to spawn new process" <<endl;
            cout << "[before spawn allComm0: parent " << parentRankInAllComm <<"] size of allCommo:" <<sizeAllComm <<endl;

			//下面的if-else区域都会产生新的tmpComm
            if(i == 0)
            {
                MPI_Comm_spawn(childProgram, argv_array, 2, MPI_INFO_NULL, 0, allComm0, &tmpInterComm0, MPI_ERRCODES_IGNORE);
            }
            else
            {
                //注意这里的参数i不能写作parentRankInWorld
                //因为每个进程都要spawn同一个i
                MPI_Comm_spawn(childProgram, argv_array, 1, MPI_INFO_NULL, i, allComm0, &tmpInterComm0, MPI_ERRCODES_IGNORE);
            }
			

            MPI_Comm_rank(allComm0, &parentRankInAllComm);
            MPI_Comm_size(allComm0, &sizeAllComm);
            cout << "[before merge allComm0: parent " << parentRankInAllComm <<"] is going to merge new process" <<endl;
            cout << "[before merge allComm0: parent " << parentRankInAllComm <<"] size of allCommo:" <<sizeAllComm <<endl;

			//这里要生成一个新的interComm
			//覆盖初始的allComm
            MPI_Intercomm_merge(tmpInterComm0, false, &allComm0);
            

            MPI_Comm_rank(allComm0, &parentRankInAllComm);
            MPI_Comm_size(allComm0, &sizeAllComm);
            cout << "[parent " << parentRankInAllComm <<"] has spawned new process" <<endl;
            cout << "[parent " << parentRankInAllComm <<"] size of allComm:" <<sizeAllComm <<endl;

        }

    }
    //如果当前进程为child，则直接和parent进行merge通信
    else
    {
        //打印所有的参数，确认从MPI_Comm_spawn传递过来的参数从第几个开始
        //for(int i =0 ; i<argc;i++)
        //{
        //    printf("spawned argument: %s\n",argv[i]);
        //}
        
        parentProcNum = atoi(argv[1]);

        //当前进程一共需要spawn多少次
        int spawningTime ;

		//只有这里merge的时候设置为true，因为新进程所属的group现在只有自己,把自己的rank号放在后面，不会干扰父进程在comm当中的rank
        MPI_Intercomm_merge(tmpInterComm0,true, &allComm0);
		
        int rankInAllComm0;

        MPI_Comm_rank(allComm0, &rankInAllComm0);

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

		//注意这里不能直接使用等于 否则最后一个进程进来之后由于只执行一些输出代码，然后就进入free部分。如果直接使用负值的话，free了allComm0之后再free allComm0就会报错invalid communicator
		//allComm0 = allComm0;
		//MPI_Comm_group(allComm0, &tmpGrp);
		//MPI_Comm_create(allComm0, tmpGrp, &allComm0);

        //如果当前进程不是最后一个子进程，它需要参与spawn其他的子进程
        if(spawningTime != 0)
        { 

            char parentProc[20];
            sprintf(parentProc, "%d", parentProcNum);
            char** argv_array = new char*[2];
            argv_array[0] = parentProc;
            argv_array[1] = NULL;

            for(int i = 0 ; i < spawningTime ;i++)
            {

                MPI_Comm_rank(allComm0, &parentRankInAllComm);
                MPI_Comm_size(allComm0, &sizeAllComm);
                cout << "[child " << parentRankInAllComm <<"] is going to spawn new process before barrier" <<endl;
                cout << "[child " << parentRankInAllComm <<"] size of allComm before barrier:" <<sizeAllComm <<endl;


                //注意这里调用的spawn，root进程的rank一定要与parent当中的保持一致。
                MPI_Comm_spawn(childProgram, argv_array, 1, MPI_INFO_NULL, parentProcNum-spawningTime+i, allComm0, &tmpInterComm0, MPI_ERRCODES_IGNORE);
				cout << "parentProcNum-spawningTime+i:"<< parentProcNum-spawningTime+i << endl;	

				//这里可能需要把allComm0改为allComm1，tmpInterComm来自于allComm0，如果输出覆盖allComm0，不知道会不会有问题
				//已实现证明，不会有问题
				//这里设置为false 由于在merge的时候当前进程所属的group是所有父进程和子进程一起的大group，这个group放在前面，所以设置为false
                MPI_Intercomm_merge(tmpInterComm0, false, &allComm0);
					
                MPI_Comm_rank(allComm0, &parentRankInAllComm);
                MPI_Comm_size(allComm0, &sizeAllComm);
                cout << "[child " << parentRankInAllComm <<"] has spawned new process" <<endl;
                cout << "[child " << parentRankInAllComm <<"] size of allComm:" <<sizeAllComm <<endl;
            }
        }
        //如果当前进程是最后一个被spawn的子进程,则不需要参与spawn新进程，只需要做一些输出即可。
        else
        {

            int parentRankInAllComm;
            int sizeAllComm;
            MPI_Comm_rank(allComm0, &parentRankInAllComm);
            MPI_Comm_size(allComm0, &sizeAllComm);
            cout << "[child " << parentRankInAllComm <<"] has spawned new process" <<endl;
            cout << "[child " << parentRankInAllComm <<"] size of allComm:" <<sizeAllComm <<endl;

        }

    }

    //到这里已经完成了所有进程的spawn和合并到allComm当中。
	//开始对每个进程进行分组
	MPI_Comm MASTER;
	MPI_Comm HEMI_A;
	MPI_Comm HEMI_B;

	MPI_Group masterGrp;
	MPI_Group hemiAGrp;
	MPI_Group hemiBGrp;
	MPI_Group allGrp;

	//获取在完成了所有的spawn之后整个comm的size
	int allCommSize;
	int allCommRank;
	MPI_Comm_size(allComm0, &allCommSize);
	MPI_Comm_rank(allComm0, &allCommRank);
	MPI_Comm_group(allComm0, &allGrp);

	//每个hemi的大小：allComm去掉0号进程之后剩下的进程平分
	int hemiSize = (allCommSize - 1) / 2 ;
	
	int *masterRanks = new int[1];
	int *hemiARanks = new int[hemiSize];
	int *hemiBRanks = new int[hemiSize];

	masterRanks[0] = 0;

	for(int i = 0; i < hemiSize; i++)
	{
		hemiARanks[i] = 2 * i + 1;
		hemiBRanks[i] = 2 * i + 2;
	}
	
	//下面这一步也可以用MPI_Comm_split做
	MPI_Group_incl(allGrp, 1, masterRanks, &masterGrp);
	MPI_Group_incl(allGrp, hemiSize, hemiARanks, &hemiAGrp);
	MPI_Group_incl(allGrp, hemiSize, hemiBRanks, &hemiBGrp);

	MPI_Comm_create(allComm0, masterGrp, &MASTER);
	MPI_Comm_create(allComm0, hemiAGrp, &HEMI_A);
	MPI_Comm_create(allComm0, hemiBGrp, &HEMI_B);



	//开始释放临时变量占据的不需要的空间
	delete[] masterRanks;
	delete[] hemiARanks;
	delete[] hemiBRanks;

	MPI_Group_free(&masterGrp );
	MPI_Group_free(&hemiAGrp );
	MPI_Group_free(&hemiBGrp );
	MPI_Group_free(&allGrp );

//**********到这里已经完成了所有的spawn以及进程分组的工作******
//**********接下来可以执行THUNDER的任务************************
	

	//注意：如果当前进程不属于某个comm，还进行Barrier，Comm_free这个comm的操作，就会报错：Comm为NULL。
	//master进程
	if(allCommRank == 0)
		MPI_Comm_free(&MASTER);
	//进程rank为奇数
	if(allCommRank%2 == 1)
		MPI_Comm_free(&HEMI_A);
	//进程rank为偶数
	if(allCommRank%2 == 2)
		MPI_Comm_free(&HEMI_B);


    MPI_Group_free(&worldGrp);
	if(tmpGrp!=MPI_GROUP_NULL)
	{
		MPI_Group_free(&tmpGrp);
	}

	if(allComm0 != MPI_COMM_NULL)
	{
		MPI_Comm_free(&allComm0 );
	}


	if( tmpInterComm0 != MPI_COMM_NULL)
	{
		MPI_Comm_free(&tmpInterComm0 );
	}


    MPI_Finalize();
}

