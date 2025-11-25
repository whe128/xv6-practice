#include "types.h"
#include "user.h"

int main(int argc, char *argv[]){
	int pipe1[2];	//store the parent to child pipe description
	int pipe2[2];	//store the child to parent pipe description.
					//[0] for read
					//[1] for write
	char buf;
	int pid;

	// create pipe
	if( pipe(pipe1) < 0 || pipe(pipe2)  < 0 ){
		printf(2, "pipe create error\n");
		exit();
	}

	//creat child process
	pid = fork();
	if(pid < 0){
		printf(2, "fork error\n");
		exit();
	}

	if(pid == 0){
		// child process
		// use pipe1 read and pipe2 write
		close(pipe1[1]);		//parent to children, close write
		close(pipe2[0]);		//children to parent, close read

		read(pipe1[0], &buf, 1);		//receive one character

		printf(1, "%d: received ping\n", getpid());

		write(pipe2[0], "p", 1);

		close(pipe1[0]);
		close(pipe2[1]);

		exit();		//child process close the process actively

	} else {
		// parent process
		// use pipe1 write and pipe2 read
		close(pipe1[0]);
		close(pipe2[1]);

		write(pipe1[1], "q", 1);

		read(pipe2[0], &buf, 1);
		printf(1, "%d: received pong\n", getpid());

		wait();		//wait for the close of child process

		close(pipe1[1]);
		close(pipe2[0]);
	}
	exit();			// parent process will run this
}
