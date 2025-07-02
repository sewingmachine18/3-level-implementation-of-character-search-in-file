#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define P 100


int search(char c, long size, int i){
	long seg = size/P;
	lseek(2, (off_t)(i*seg), SEEK_SET);
	char buff[1024];
	ssize_t rcnt;
	int cnt = 0; int check = 0;

	//last child
	if(i == P-1){
		do{
			rcnt = read(2, buff, sizeof(buff));
			if(rcnt == -1){
				perror("read_in_child");
				exit(1);
			}
			for(int j = 0; j < rcnt; ++j){
				if(buff[j] == c) { ++cnt;}
			}
		}while(rcnt != 0);

		return cnt;
	}


	//the other children
	int space;
	while(check < seg){
		space = sizeof(buff);
		if(space > (seg-check)) space = (seg-check);
		rcnt = read(2, buff, space);
		if(rcnt == -1){
			perror("read_in_child");
			exit(1);
		}
		for(int j = 0; j < rcnt; ++j){
			if(buff[j] == c) { ++cnt; }
		}
		check += rcnt;
    }
        return cnt;
}

int main(int argc, char *argv[]){
	char c = argv[1][0];
	long size = atol(argv[2]);
	int i = atoi(argv[3]);
	int k;
	int res = search(c, size, i);
	//printf("got the jpb\n");
	write(1, &res, sizeof(res));
	while(1){
		read(0, &k, sizeof(k));
		//printf("%d->%d\n", i, k);
		int res = search(c, size, k);
		write(1, &res, sizeof(res));
	}
}

