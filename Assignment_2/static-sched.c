#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

int work(int w){
	sleep(w % 10);
	return w*w;
}

int main(int argc, char **argv){
	int rank, size, n;
	
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	
	// process 0 reads input
	if (rank == 0){
		printf("> n: ");
		fflush(stdout);
		scanf("%d", &n);
	}
	
	// process 0 sends n to all others
	MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
	MPI_Barrier(MPI_COMM_WORLD);
	double start_time = MPI_Wtime();
	
	
	//  computing iterations per process
	int chunk = n / size;
	int remaining = n % size;
	
	// processes with rank < remaining get 1 extra rep
	int local_count = chunk + (rank < remaining ? 1 : 0);
	int start = rank*chunk + (rank < remaining ? rank : remaining);
	int end = start + local_count;
	
	int *local_res = (int *) malloc(local_count * sizeof(int));
	
	// every process execs reps
	for (int i = start, j = 0; i<end; ++i, ++j){
		local_res[j] = work(i);
	}
	
	// gather results
	if (rank == 0){
		int *result = (int *) malloc(n*sizeof(int));
		
		// process 0 copies its own results
		for (int i=0; i<local_count; i++){
			result[start+i] = local_res[i];
		}
		
		// receive results from other processes 
		for (int p=1; p <size; p++){
			int p_count = chunk + (p <remaining ? 1:0);
			int p_start = p * chunk + (p < remaining ? p : remaining);
			MPI_Recv(&result[p_start], p_count, MPI_INT, p, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		
		for (int i=0; i<n; i++){
			printf("work(%d) = %d\n", i, result[i]);
		}
		free(result);
	}else{
		// all other processes send local array to process 0
		MPI_Send(local_res, local_count, MPI_INT, 0, 0, MPI_COMM_WORLD);
	}
	
	MPI_Barrier(MPI_COMM_WORLD);
	double end_time = MPI_Wtime();
	
	// process 0 computes execution time
	if (rank==0){
		double elapsed_time = end_time - start_time;
		printf("\n Experiment (n=%d, P= %d) completed in %f seconds.\n", n, size, elapsed_time);
	}
	
	free(local_res);
	MPI_Finalize();
	return 0;
}