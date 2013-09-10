/* fork and exec a child, wait for cleanup
 * Kacper Wysocki */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

pid_t pid;
char *cleanup = "";
void trapper(int sig)
{
	kill(pid, sig);
	system(cleanup);
}

int main(int argc, char **argv, char **envp)
{
	int status, rc;

	if(argc < 3){
		fprintf(stderr, "error: what u want from meeeee!?\n");
		fprintf(stderr, "usage: %s <cleanup> <cmd args>\n", argv[0]);
		exit(1);
	}
	cleanup = argv[1];
	char **execargs = argv+2;

	pid = fork();

	if (pid > 0) {
		signal(SIGINT, trapper);
		rc = waitpid(pid, &status, 0);
		if(rc == -1){
			fprintf(stderr, "Couldn't wait for child %d, %s\n", pid, strerror(errno));
			exit(1);
		}else if(WIFEXITED(status)){
			if(WEXITSTATUS(status) != 0){
				fprintf(stderr, "Child failed with exit status %d\n", WEXITSTATUS(status));
			}
		}else if(WIFSIGNALED(status)){
			fprintf(stderr, "Child pid terminated by signal %d\n", WTERMSIG(status));
		}
		rc = system(cleanup);
		exit(rc);
	}
	if (pid < 0) {
		fprintf(stderr, "Failed to fork!\n");
		exit(-pid);
	}
	/*
	 * new process group
	 */
	//setsid();
	rc = execve(execargs[0], execargs, envp);
	if(rc){
		fprintf(stderr, "Failed to exec %s, %s\n", execargs[0], strerror(errno));
	}
}
