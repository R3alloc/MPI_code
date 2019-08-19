#include "mpi.h"
#include <stdio.h>

int main()
{
	MPI_Init(NULL, NULL);

	// Get the rank and size in the original communicator
	int world_rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	
	// Get the group of processes in MPI_COMM_WORLD
	MPI_Group world_group;
	MPI_Comm_group(MPI_COMM_WORLD, &world_group);
	
	int n = 3;
	const int ranks[3] = {1, 3, 5};
	
	// Construct a group containing all of the prime ranks in world_group
	MPI_Group prime_group;
	MPI_Group_incl(world_group, 3, ranks, &prime_group);
	
	// Create a new communicator based on the group
	MPI_Comm prime_comm;
	//从group当中创建communicator。如果当前进程的rank不属于prime_group，则prime_comm被赋值为MPI_COMM_NULL
	MPI_Comm_create_group(MPI_COMM_WORLD, prime_group, 0, &prime_comm);
	
	int prime_rank = -1, prime_size = -1;
	// If this rank isn't in the new communicator, it will be
	// MPI_COMM_NULL. Using MPI_COMM_NULL for MPI_Comm_rank or
	// MPI_Comm_size is erroneous
	if (MPI_COMM_NULL != prime_comm) 
	{
		MPI_Comm_rank(prime_comm, &prime_rank);
	   	MPI_Comm_size(prime_comm, &prime_size);
	}
	
	printf("WORLD RANK/SIZE: %d/%d \t PRIME RANK/SIZE: %d/%d\n",
		world_rank, world_size, prime_rank, prime_size);
	
	//MPI_Barrier(prime_comm);
	MPI_Group_free(&world_group);
	MPI_Group_free(&prime_group);
	//注意这里一定要先判定create的communicator是否成功，返回的是不是MPI_COMM_NULL。
	//如果返回的是MPI_COMM_NULL，则在free的时候会发生fatal error
	if(MPI_COMM_NULL != prime_comm)
	{
		printf("WORLD RANK/SIZE: %d/%d \t PRIME RANK/SIZE: %d/%d ,Call MPI_Comm_free(&prime_comm)\n",
				world_rank, world_size, prime_rank, prime_size);
		MPI_Comm_free(&prime_comm);
	}
	MPI_Finalize();
}
