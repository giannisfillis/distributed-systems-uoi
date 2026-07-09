#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#define TAG_TASK 1
#define TAG_RESULT 2
#define TAG_DIE 3

int work(int w){
	sleep(w % 10);
	return w*w;
}

int main(int argc, char **argv){
	int rank, size, n;
	
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	

	// the master reads the n value 
	if (rank == 0){
		printf("> n: ");
		fflush(stdout);
		scanf("%d", &n);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	double start_time = MPI_Wtime();
		
		
	if (rank == 0){
		// this is what the master executes (process 0)
		int *result = (int*) malloc(n* sizeof(int));
		int next_task = 0;
		int active_workers = 0;
			
		// initially, send 1 iteration per process
		for (int p=1; p < size && next_task < n; p++){
			MPI_Send(&next_task, 1, MPI_INT, p, TAG_TASK, MPI_COMM_WORLD);
			next_task++;
			active_workers++;
		}
			
		// receive results and dynamic delegation
		while (active_workers > 0){
			int recv_buf[2]; // pos 0 is the index of repetion, pos 1 is the result 
			MPI_Status status;
				
			// wait result from any worker
			MPI_Recv(recv_buf, 2, MPI_INT, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);
			result[recv_buf[0]] = recv_buf[1];
				
			if (next_task < n){
				MPI_Send(&next_task, 1, MPI_INT, status.MPI_SOURCE, TAG_TASK, MPI_COMM_WORLD);
				next_task++;
			}else{
				// send signal to terminate
				int dummy = -1;
				MPI_Send(&dummy, 1, MPI_INT, status.MPI_SOURCE, TAG_DIE, MPI_COMM_WORLD);
				active_workers--;
			}	
		}
		// edge case if there are workers not working
		for (int p= (n <size-1 ? n+1 : size); p <size; p++){
			int dummy = -1;
			MPI_Send(&dummy, 1, MPI_INT, p, TAG_DIE, MPI_COMM_WORLD);
		}
			
		for (int i=0; i<n; ++i){
			printf("work(%d) = %d \n", i, result[i]);
		}
		free(result);
	}else{
		// this is what workers execute 
		while(1){
			int task;
			MPI_Status status;
				
			// wait for message from the master to see what to do 
			MPI_Recv(&task, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				
			// if it tells you to die,die 
			if(status.MPI_TAG == TAG_DIE){
				break;
			}
				
			// execute the work ones
			int res = work(task);
				
			// put repetion index and result and send it 
			int send_buf[2] = {task, res};
			MPI_Send(send_buf, 2, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
		}
	}
		
	MPI_Barrier(MPI_COMM_WORLD);
	double end_time = MPI_Wtime();
	
	// master computes execution time
	if (rank==0){
		double elapsed_time = end_time-start_time;
		printf("\n Experiment (n=%d, P= %d) completed in %f seconds.\n", n, size, elapsed_time);
	}
	
	MPI_Finalize();
	return 0;
}