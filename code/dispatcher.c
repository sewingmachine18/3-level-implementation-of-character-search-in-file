#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


#define N 100
#define P 16

int main(int argc, char *argv[]){
	//printf("hola from d-man\n");

	char *file = argv[1];
	int fdr = atoi(argv[2]);
	int fdw = atoi(argv[3]);
	int fd_final = atoi(argv[4]);
	char c = argv[5][0];
	
	//unblock read() from front-end
	int flags = fcntl(fdr, F_GETFL, 0);
	fcntl(fdr, F_SETFL, flags | O_NONBLOCK);

	//read file and find its size
	int fd = open(file, O_RDONLY);
	if(fd == -1){
		perror("open2read-d");
		exit(1);
    }
	struct stat s;
	fstat(fd, &s);
	off_t sz = s.st_size;
	long size = (long)sz;
	close(fd);


	//pipes
	int pcnt[P][2];//pipe for subcounts
	for(int j = 0; j < P; ++j)
		if(pipe(pcnt[j]) < 0){
			perror("pipe for workers");
			exit(1);
		}
	int pass[P][2];//pipe for new assignments
    for(int j = 0; j < P; ++j)
		if(pipe(pass[j]) < 0){
		perror("pipe for workers");
		exit(1);
	}

	//unblock read() from workers
	for(int j = 0; j < P; ++j){
		flags = fcntl(pcnt[j][0], F_GETFL, 0);
		fcntl(pcnt[j][0], F_SETFL, flags | O_NONBLOCK);
	}
	
	//variables
	int jobs[N]; //work pool -1:being searched, 0:unsearched, 1:searched
	for(int i = 0; i < N; ++i) jobs[i] = 0;
	int staff[P]; //work list
	int worker_job[P]; // which job is assigned to each worker
	for(int i = 0; i < P; ++i) staff[i] = -1;
	char buff[512], w[8];
	w[7] = '\0';
	int ind = 0, counter = 0, status; //worker index, counter of how many jobs have been completed
	int ccnt = 0, x, k, subcnt, total = 0;//x<-workers read, k<-worker's assignment
	float progress = 0;
	pid_t p, ppid = getppid();
	while(1){
		read(fdr, buff, sizeof(buff));
		
		//show progress
		if(strcmp(buff, "show progress") == 0) {
			buff[0] = '\0';
			progress = ((float)ccnt)/size;
			progress *= 100;
			//printf("prog = %f, ccnt = %d\n", progress, ccnt);
			write(fdw, &progress, sizeof(progress));
			write(fdw, &total, sizeof(total));
		}
		
		//add workers
		else if(sscanf(buff, "add %d %7s", &x, w) == 2 && strcmp(w, "workers") == 0) {
			buff[0] = '\0';
			int t = ind;
			for(; ((ind  < x+t) && (ind < P)); ++ind){
				for(int i = 0; i < N; ++i)
					if(jobs[i] == 0) {
						k = i;
						jobs[i] = -1;
						break;
					}
				worker_job[ind] = k;
				p = fork();
				if(p < 0){
					perror("fork_worker");
					exit(1);
				}
				if(p == 0){
					dup2(pass[ind][0], 0);
					dup2(pcnt[ind][1], 1);
					close(pass[ind][0]);
					close(pass[ind][1]);
					close(pcnt[ind][0]);
					close(pcnt[ind][1]);
					int fd = open(file, O_RDONLY);
					dup2(fd, 2);
					close(fd);
					//stringify the arguments
					char sc[10], ssize[10], sk[10];
					sprintf(sc, "%c", c);
					sprintf(ssize, "%ld", size);
					sprintf(sk, "%d", k);
					char *args[] = {"./a1.4-worker", sc, ssize, sk, NULL};
					execv(args[0], args);
					exit(0);
				}
				else{
					staff[ind] = p;
				}
			}
		}
		
		//remove workers
		else if((sscanf(buff, "remove %d %7s", &x, w) == 2) && (strcmp(w, "workers") == 0)){
			buff[0] = '\0';
			int sink;
			for(int i = 0; (i < x) && (ind > 0); i++){
				if(worker_job[ind-1] >= 0) jobs[worker_job[ind-1]] = 0;
				kill(staff[ind-1], SIGKILL);
				waitpid(staff[ind-1], &status, 0);
				read(pcnt[ind-1][0], &sink, sizeof(sink));//empty the pipe
				ind--;
			}
		}

		//read from workers
		if(ind > 0){
			for(int j = 0; j < ind; ++j)
				if(read(pcnt[j][0], &subcnt, sizeof(subcnt)) > 0){
					total += subcnt;
					counter++;
					if(worker_job[j] != (N-1)) ccnt += size/N;
					else ccnt += (size - (N-1)*(size/N));
					//usleep(250000);
					jobs[worker_job[j]] = 1;
					k = -1;
					for(int i = 0; i < N; ++i)
						if(jobs[i] == 0){
							k = i;
							jobs[i] = -1;
							break;
					}
					if(k >= 0){
						worker_job[j] = k;
						write(pass[j][1], &k, sizeof(k));
					}
					else worker_job[j] = -1;
				}
		}
		if(counter == N) {
			for(int i=0; i<ind; i++){
				kill(staff[i], SIGKILL);
				waitpid(staff[i], &status, 0);
			}
			progress = 100;
			write(fdw, &progress, sizeof(progress));
			write(fdw, &total, sizeof(total));
			write(fd_final, &total, sizeof(total));
			kill(ppid, SIGUSR1);
			exit(0);
		}
	}	

	exit(1);
}
