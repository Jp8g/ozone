#pragma once

typedef struct oz_task {
	void* (*function)(void*);
	void* data;
} oz_task;

void oz_task_system_init();
void oz_task_submit(oz_task task);
void oz_task_system_shutdown();