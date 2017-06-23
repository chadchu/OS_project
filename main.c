#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <signal.h>
#include <time.h>
#include <limits.h>

#define TIME_UNIT(t){\
						volatile unsigned long _x;\
						for (_x = 0; _x < (t); _x++) {\
							volatile unsigned long _y;\
							for (_y = 0; _y < 1000000UL; _y++);\
						}\
					}

typedef struct {
	char n[32]; // n: Process name.
	int r, t;   // r: ready time, t: execution time.
	pid_t pid;  // pid: process pid
} Proc;

int N;
Proc *P, infp;
cpu_set_t cpuset_m, cpuset_c;
struct sched_param _min, _max;
struct timeval start, end;
char mesg[256];


int cmp(const void *a, const void *b) {
	Proc *A = (Proc *)a, *B = (Proc *)b;
	return A->r > B->r;
}
void FIFO(void) {
	
	/* Sort the processes by ready time. */
	qsort(P, N, sizeof(Proc), cmp);
	int t1 = 0, t2;

	int i;
	for (i = 0; i < N; i++) {
		
		t2 = P[i].r-t1;
		if (t2 > 0) {
			TIME_UNIT(t2)
			t1 += t2;
		}
		
		P[i].pid = fork();
		if (P[i].pid == 0) {
			sched_setaffinity(0, sizeof(cpu_set_t), &cpuset_c);
			gettimeofday(&start, NULL);
			TIME_UNIT(P[i].t);
			gettimeofday(&end, NULL);

			sprintf(mesg, "[Project1] %d %ld.%ld %ld.%ld\n", (int)getpid(), start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);
			syscall(342, mesg);
			exit(0);
		} else {
			sched_setscheduler(P[i].pid, SCHED_FIFO, &(_max));
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_min));
			TIME_UNIT(P[i].t);
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_max));
			waitpid(P[i].pid, NULL, 0);
			printf("%s %d\n", P[i].n, (int)P[i].pid);
		}
	}
	return;
}
void RR(void) {
	qsort(P, N, sizeof(Proc), cmp);
	#define next(x) ((x) == N-1 ? 0 : (x)+1)
	int quantum = 500;
	int head = 0, tail = 0;
	Proc **queue = (Proc **)malloc(sizeof(Proc *)*N);
	int t1 = 0, t2;
	int remain = N;
	int cnt = 0;
	while( remain ) {

		while(cnt < N && P[cnt].r <= t1) {
			queue[tail] = &P[cnt++];
			queue[tail]->pid = -1;
			tail = next(tail);
		}

		if ( remain+cnt == N ) {
			t2 = P[cnt].r - t1;
			TIME_UNIT(t2)
			t1 += t2;
		}
		while (cnt < N && P[cnt].r <= t1) {
			queue[tail] = &P[cnt++];
			queue[tail]->pid = -1;
			tail = next(tail);
		}

		if (queue[head]->pid == -1) {
			queue[head]->pid = fork();
			if (queue[head]->pid == 0) {
				sched_setaffinity(0, sizeof(cpu_set_t), &cpuset_c);
				gettimeofday(&start, NULL);
				TIME_UNIT(queue[head]->t)
				gettimeofday(&end, NULL);
				sprintf(mesg, "[Project1] %d %ld.%ld %ld.%ld\n", (int)getpid(), start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);
				syscall(342, mesg);
				exit(0);
			}
		}

		sched_setscheduler(queue[head]->pid, SCHED_FIFO, &(_max));
		sched_setscheduler(infp.pid, SCHED_FIFO, &(_min));
		
		if (queue[head]->t <= quantum) {
			TIME_UNIT(queue[head]->t)
			waitpid(queue[head]->pid, NULL, 0);
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_max));
			printf("%s %d\n", queue[head]->n, (int)queue[head]->pid);
			t1 += queue[head]->t;

			while (cnt < N  && P[cnt].r <= t1) {
				queue[tail] = &P[cnt++];
				queue[tail]->pid = -1;
				tail = next(tail);
			}
			queue[head]->t = 0;
			head = next(head);
			remain--;
		} else {
			TIME_UNIT(quantum);
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_max));
			sched_setscheduler(queue[head]->pid, SCHED_FIFO, &(_min));
			t1 += quantum;
			while(cnt < N && P[cnt].r <= t1) {
				queue[tail] = &P[cnt++];
				queue[tail]->pid = -1;
				tail = next(tail);
			}
			queue[head]->t -= quantum;
			queue[tail] = queue[head];
			head = next(head);
			tail = next(tail);
		}
	}
	free(queue);
}
void SJF(void) {
	
	int ready = 0, cnt = 0;

	qsort(P, N, sizeof(Proc), cmp);
	
	int t1 = 0, t2;
	int i;
	for (i = 0; i < N; i++) {
		while(cnt < N && P[cnt].r <= t1) cnt++, ready++;
		if ( !ready ) {
			t2 = P[cnt].r - t1;
			TIME_UNIT(t2)
			t1 += t2;
		}
		while(cnt < N && P[cnt].r <= t1) cnt++, ready++;
		Proc *job = NULL;
		int j;

		for (j = 0; j < cnt; j++) {
			if (P[j].t != 0) {
				if (job == NULL) job = &P[j];
				else if (P[j].t < job->t) job = &P[j];
			}
		}
		job->pid = fork();
		if (job->pid == 0) {
			sched_setaffinity(0, sizeof(cpu_set_t), &cpuset_c);
			gettimeofday(&start, NULL);
			TIME_UNIT(job->t);
			gettimeofday(&end, NULL);
			sprintf(mesg, "[Project1] %d %ld.%ld %ld.%ld\n", (int)getpid(), start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);
			syscall(342, mesg);
			exit(0);
		} else {
			sched_setscheduler(job->pid, SCHED_FIFO, &(_max));
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_min));
			TIME_UNIT(job->t)
			t1 += job->t;
			job->t = 0;
			ready--;
			waitpid(job->pid, NULL, 0);
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_max));
			printf("%s %d\n", job->n, (int)job->pid);
		}
	}
}
void PSJF(void) {
	qsort(P, N, sizeof(Proc), cmp);
	int t1 = 0, t2; // t1: 目前時間
	int cnt = 0, remain = N; // cnt: 前幾項已經ready, remain: 還有幾項沒有執行完畢(包含還沒ready的process)
	while ( remain ) {
		while(cnt < N && P[cnt].r <= t1) P[cnt++].pid = -1;
		if ( remain+cnt == N ) {
			t2 = P[cnt].r - t1;
			TIME_UNIT(t2)
			t1 += t2;
		}
		while(cnt < N && P[cnt].r <= t1) P[cnt++].pid = -1;
		Proc *job = NULL;
		int j;
		for (j = 0; j < cnt; j++) {
			if (P[j].t) {
				if (job == NULL) job = &P[j];
				else if (P[j].t < job->t) job = &P[j];
			}
		}
		if (job->pid == -1) {
			job->pid = fork();
			if (job->pid == 0) {
				sched_setaffinity(0, sizeof(cpu_set_t), &cpuset_c);
				gettimeofday(&start, NULL);
				TIME_UNIT(job->t)
				gettimeofday(&end, NULL);
				sprintf(mesg, "[Project1] %d %ld.%ld %ld.%ld\n", (int)getpid(), start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);
				syscall(342, mesg);
				exit(0);
			}
		}
		/* t2: 保守可執行時間，設為“目前再過多久會有process get ready” */
		t2 = job->t;
		if (cnt < N && (P[cnt].r <  t1+t2)) t2 = P[cnt].r - t1;

		sched_setscheduler(job->pid, SCHED_FIFO, &(_max));
		sched_setscheduler(infp.pid, SCHED_FIFO, &(_min));
		
		if (t2 == job->t) {
			TIME_UNIT(t2)
			waitpid(job->pid, NULL, 0);
			remain--;
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_max));
			printf("%s %d\n", job->n, (int)job->pid);
			t1 += t2;
			job->t = 0;
		} else {
			TIME_UNIT(t2)
			sched_setscheduler(infp.pid, SCHED_FIFO, &(_max));
			sched_setscheduler(job->pid, SCHED_FIFO, &(_min));
			t1 += t2;
			job->t -= t2;
		}
	}
}
int main(void) {
	
	CPU_ZERO(&cpuset_m);
	CPU_ZERO(&cpuset_c);
	CPU_SET(0, &cpuset_m);
	CPU_SET(1, &cpuset_c);

	_min.sched_priority = sched_get_priority_min(SCHED_FIFO);
	_max.sched_priority = sched_get_priority_max(SCHED_FIFO);
	
	sched_setaffinity(0, sizeof(cpu_set_t), &cpuset_m);

	infp.pid = fork();
	if (infp.pid == 0) {
		sched_setaffinity(0, sizeof(cpu_set_t), &cpuset_c);
		while(1);
	}
	/*將優先序設為最高*/
	sched_setscheduler(infp.pid, SCHED_FIFO, &(_max));
	
	/* Input */
	char policy[32];
	scanf("%s%d", policy, &N);
	P = (Proc *)malloc(sizeof(Proc)*N);
	int i;
	for (i = 0; i < N; i++) {
		scanf("%s%d%d", P[i].n, &P[i].r, &P[i].t);
	}
	
	/* Scheduling */
	if ( !strcmp("FIFO", policy) ) FIFO();
	if ( !strcmp("RR", policy) ) RR();
	if ( !strcmp("SJF", policy) ) SJF();
	if ( !strcmp("PSJF", policy) ) PSJF();
	
	/* terminate 'infp' */
	kill(infp.pid, SIGKILL);
	waitpid(infp.pid, NULL, 0);	
	return 0;
}