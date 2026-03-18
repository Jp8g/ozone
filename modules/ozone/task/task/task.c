#include <ozone/task/task.h>
#include <ozone/os/thread/thread.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
bool shutdown = 0;
oz_task* tasks;
uint32_t taskCount = 0;
uint32_t taskCapacity = 0;

void* thread_loop(void* args) {
	while (1) {
		pthread_mutex_lock(&lock);

		while (!taskCount && !shutdown) {
			pthread_cond_wait(&cond, &lock);
		}

		if (shutdown) {
			pthread_mutex_unlock(&lock);
			return NULL;
		}

		taskCount--;
		void* task_data = tasks[taskCount].data;
		void* (*task_function)(void*) = tasks[taskCount].function;
		pthread_mutex_unlock(&lock);

		task_function(task_data);
		pthread_mutex_lock(&lock);
		pthread_mutex_unlock(&lock);
	}
}

void oz_task_system_init() {
	long threadCount = oz_get_thread_count();

	for (long i = 0; i < threadCount - 2; i++) {
		pthread_t thread;

		pthread_create(&thread, NULL, thread_loop, NULL);
	}

	taskCapacity = 1;
	tasks = malloc(taskCapacity * sizeof(oz_task));
}

void oz_task_submit(oz_task task) {
	pthread_mutex_lock(&lock);

	taskCount++;
	if (taskCount > taskCapacity) {
		taskCapacity *= 2;
		tasks = realloc(tasks, taskCapacity * sizeof(oz_task));
	}

	tasks[taskCount - 1] = task;

	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);
}

void oz_task_system_shutdown() {
	pthread_mutex_lock(&lock);
	shutdown = true;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&lock);

	free(tasks);
}