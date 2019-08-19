//注：本代码在运行的过程中故意安排了一些问题

#include <stdio.h>
#include "mpi.h"
int main()
{

	//Examples: Divide MPI tasks into two groups
	int rank, numtasks;
	MPI_Group orig_group, new_group = MPI_GROUP_NULL;
	MPI_Comm new_comm;

	MPI_Init(NULL, NULL);
	//获取当前rankID
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	//获取所有task的数量
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

	//Exrtract the original group handle
	MPI_Comm_group(MPI_COMM_WORLD, &orig_group);

	//Divid tasks into two distinct groups based upon rank
	int ranks1[4] = {0,1,2,3};
	int ranks2[4] = {5,6,7,8};

	//若当前rankID < 4,则将0，1，2，3号(也就是ranks1)一共4个task（也就是numtasks/2)生成一个new_group
	if(rank < numtasks/2)
	{
		MPI_Group_incl(orig_group, numtasks/2, ranks1, &new_group);
		printf("Rank %d is included in subgroup with ranks1\n",rank);
	}
	else if(rank > numtasks/2)
	{
		MPI_Group_incl(orig_group, numtasks/2, ranks2, &new_group);
		printf("Rank %d is included in subgroup with ranks2\n",rank);
	}
	//task 4 没有被包含进入任何subgroup
	else
	{
		printf("Rank %d is not included in any subgroup\n",rank);
	}

	//Create new communicator and Brodcast within the new group
	//根据上面生成的new_group，再生成对应的communicator
	//注意task4也没有对应的new_comm
	if(new_group != MPI_GROUP_NULL)
	{
		MPI_Comm_create(MPI_COMM_WORLD, new_group, &new_comm);
	}

	//注意这一部分，在这里设置了只有成功create了new_group的task才执行。
	//但是MPI_Barrier正是需要所有的task都调用同一个API，才能完成整个程序的运行。
	//但是如果不写这个if语句的话，由于task4没有belong的group，也就无法create comm，在barrier的时候实际上在barrier一个null comm，就会报错
	if(new_comm != MPI_COMM_NULL && new_group != MPI_GROUP_NULL)
	{
		MPI_Barrier(new_comm);
	}

	MPI_Finalize();
} 

