#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define P 16

int flag = 1;

void show_pstree(pid_t p){
	int ret;
	char cmd[1024];

	snprintf(cmd, sizeof(cmd), "echo; echo; pstree -G -c -p %ld; echo; echo", (long)p);
	cmd[sizeof(cmd)-1] = '\0';
	ret = system(cmd);
	if(ret < 0){
		perror("system");
		exit(104);
	}
}

void handler(int sig){
	flag = 0;
}


int main(int argc, char *argv[]) {
		
	//signal from dispatcher
	signal(SIGUSR1, handler);
	char intro[] = "Add workers to begin.\nThe number of workers will remain between 0 and 16 at all times.\nGiven a command that suggests otherwise, the closest value to that will be applied.\n";
	if(write(1, intro, sizeof(intro)) < 0){
		perror("starting message");
		exit(1);
	}
	char c = argv[2][0];

	//unblock read from stdin
	int flags = fcntl(0, F_GETFL, 0);
	fcntl(0, F_SETFL, flags | O_NONBLOCK);

	//pipe1 for write, pipe2 for read, pipe3 for finish
	int pfd1[2];
	pipe(pfd1);
	int pfd2[2];
	pipe(pfd2);
	int pfd3[2];
	pipe(pfd3);

	//create dispatcher
	pid_t p;
	p = fork();
	if(p < 0){
		perror("fork dispatcher");
		exit(1);
	}
	if(p == 0){
		close(pfd1[1]);
		close(pfd2[0]);
		close(pfd3[0]);

		char sfdr[10], sfdw[10], sfd_final[10], sc[10];
		sprintf(sfdr, "%d", pfd1[0]);
		sprintf(sfdw, "%d", pfd2[1]);
		sprintf(sfd_final, "%d", pfd3[1]);
		sprintf(sc, "%c", c);
		char *args[] = {"./a1.4-dispatcher", argv[1], sfdr, sfdw, sfd_final, sc, NULL};
		execv(args[0], args);
	}


	//front_end
	else{
		//variables
		float per;
		int found;

		close(pfd1[0]);
		close(pfd2[1]);
		close(pfd3[1]);
		ssize_t rcnt;
		char buff[512];
	
		//pending for command from user
		while(flag) {
			rcnt = read(0, buff, sizeof(buff)-1);
			if(rcnt > 0) { buff[rcnt-1] = '\0'; }
			
			//show process info
			if(strcmp(buff, "show process info") == 0){
				show_pstree(p);
			}
			//ask about progress
			else if(strcmp(buff, "show progress") == 0){
				if(write(pfd1[1], buff, sizeof(buff)) < 0){
					perror("progress message");
					exit(1);
				}
				rcnt = read(pfd2[0], &per, sizeof(per));
				if(rcnt == -1){
					perror("read progress");
					exit(1);
				}
				rcnt = read(pfd2[0], &found, sizeof(found));
				if(rcnt == -1){
					perror("read progress");
					exit(1);
				}
				sprintf(buff, "%f% work done, characters found so far = %d\n", per, found);
				write(1, buff, strlen(buff));
			}

			// remove or add workers
			else if(rcnt >0) {
				if(write(pfd1[1], buff, sizeof(buff)) < 0){
					perror("add or rm message");
					exit(1);
				}
			}
			buff[0] = '\0';
		}

		//after finishinig
		int res;
		rcnt = read(pfd3[0], &res, sizeof(res));
		if(rcnt == -1){
			perror("read progress");
			exit(1);
		}
		sprintf(buff, "The character %c appears in file %s %d times.\n", c, argv[1], res);
		write(1, buff, strlen(buff));
		int status;
		wait(&status);
	}
	return 0;
}
