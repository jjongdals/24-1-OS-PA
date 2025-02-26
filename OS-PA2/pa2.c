/**********************************************************************
 * Copyright (c) 2019-2024
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;

/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;

/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];

/**
 * Monotonically increasing ticks. Do not modify it
 */
extern unsigned int ticks;

/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;

/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
static bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_BLOCKED;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
static void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_BLOCKED);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);


		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

#include "sched.h"

/***********************************************************************
 * FCFS scheduler
 ***********************************************************************/
static int fcfs_initialize(void)
{
	return 0;
}

static void fcfs_finalize(void)
{
}

static struct process *fcfs_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, the current process can be blocked
	 * while acquiring a resource. In this case just pick the next as well.
	 */
	if (!current || current->status == PROCESS_BLOCKED) {
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) {
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note that we use 
		 * list_del_init() over list_del() to maintain the list head tidy.
		 * Otherwise, the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the process to run next */
	return next;
}

struct scheduler fcfs_scheduler = {
	.name = "FCFS",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fcfs_initialize,
	.finalize = fcfs_finalize,
	.schedule = fcfs_schedule,
};

/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	/**
	 * Implement your own SJF scheduler here.
	 */
	struct process *next = NULL;
	struct process *pos = NULL;

	if(!current || current->status == PROCESS_BLOCKED) {
		goto pick_next;
	}

	if(current->age < current->lifespan) {
		return current;
	}

pick_next:
	if(!list_empty(&readyqueue)) {
		next = list_first_entry(&readyqueue, struct process, list);
		list_for_each_entry(pos, &readyqueue, list) {
			if(pos->lifespan < next->lifespan) {
				next = pos;
			}
		}
		list_del_init(&next->list);
	}

	return next;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire,	/* Use the default FCFS acquire() */
	.release = fcfs_release,	/* Use the default FCFS release() */
	.schedule = sjf_schedule,			/* TODO: Assign your schedule function  
								   to this function pointer to activate
								   SJF in the simulation system */
};

/***********************************************************************
 * STCF scheduler
 ***********************************************************************/
static struct process *stcf_schedule() {

	struct process *next = NULL;
	struct process *pos = NULL;

	if(!current || current->status == PROCESS_BLOCKED) {
		goto pick_next;
	}
	//age랑 lifespan이랑 안 같으면 계속 실행하니까, preemptive한 특성 추가
	if(current->age < current->lifespan) {
		list_add_tail(&current->list, &readyqueue);
	}
	
pick_next:
	if(!list_empty(&readyqueue)) {
		next = list_first_entry(&readyqueue, struct process, list);
		list_for_each_entry(pos, &readyqueue, list) {
			// life span 남은 건 계속 계산 해줘야 함 (age에서 빼줘야 함 남은 life span 계산이니)
			unsigned int remain_lifespan = pos->lifespan - pos->age;
			unsigned int next_lifespan = next->lifespan - next->age;
			if(remain_lifespan < next_lifespan) {
				next = pos;
			}
		}
		list_del_init(&next->list);
	}
	return next;
}

/***********************************************************************
 * STCF scheduler
 ***********************************************************************/
struct scheduler stcf_scheduler = {
	.name = "Shortest Time-to-Complete First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	/* You need to check the newly created processes to implement STCF.
	 * Have a look at @forked() callback.
	 */
	.schedule = stcf_schedule,
};

/***********************************************************************
 * round robin schedule
 ***********************************************************************/
static struct process *rr_schedule() {

	struct process *next = NULL;

	if(!current || current->status == PROCESS_BLOCKED) {
		goto pick_next;
	}

	if(current->age < current->lifespan) {
		list_add_tail(&current->list, &readyqueue);
	}
	
pick_next:
	if(!list_empty(&readyqueue)) {
		next = list_first_entry(&readyqueue, struct process, list);
		list_del_init(&next->list);
	}

	return next;

}
/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = rr_schedule, // rr_schedule use
	/* Obviously, ... */
};

static void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);
		struct process *pos = NULL;
		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_BLOCKED);

		list_for_each_entry(pos, &r->waitqueue, list) {
			if(waiter->prio < pos->prio) {
				waiter = pos;
			}
		}

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);


		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

/***********************************************************************
 * priority schedule
 ***********************************************************************/
static struct process *prio_schedule() {

	struct process *next = NULL;
	struct process *pos = NULL;

	if(!current || current->status == PROCESS_BLOCKED) {
		goto pick_next;
	}

	if(current->age < current->lifespan) {
		list_add_tail(&current->list, &readyqueue);
	}
	
pick_next:
	if(!list_empty(&readyqueue)) {
		next = list_first_entry(&readyqueue, struct process, list);
		list_for_each_entry(pos, &readyqueue, list) {
			if(next->prio < pos->prio) {
				next = pos;
			}
		}
		list_del_init(&next->list);
	}
	return next;
}

/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = fcfs_acquire,
	.release = prio_release,
	.schedule = prio_schedule,
};

/***********************************************************************
 * priority + aging schedule
 ***********************************************************************/
static struct process *pa_schedule() {

	struct process *next = NULL;
	struct process *pos = NULL;

	if(!current || current->status == PROCESS_BLOCKED) {
		goto pick_next;
	}

	if(current->age < current->lifespan) {
		list_add_tail(&current->list, &readyqueue);
	}
	
pick_next:
	if(!list_empty(&readyqueue)) {
		next = list_first_entry(&readyqueue, struct process, list);
		list_for_each_entry(pos, &readyqueue, list) {
			// test case에서 같다고 안하면 틀림 왜지?
			if(next->prio <= pos->prio) {
				next = pos;
			}
			pos->prio++;
		}
		next->prio = next->prio_orig;
		list_del_init(&next->list);
	}

	return next;
}

/***********************************************************************
 * Priority scheduler with aging
 ***********************************************************************/
struct scheduler pa_scheduler = {
	.name = "Priority + aging",
	.acquire = fcfs_acquire,
	.release = prio_release,
	.schedule = pa_schedule,
};

static bool pcp_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		// 지금 리소스를 쓰고 있는 애를 max prio로 설정 말곤 다른 추가할 건 없음
		r->owner->prio = MAX_PRIO;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_BLOCKED;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

static void pcp_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);
	//prio 복구
	r->owner->prio = r->owner->prio_orig;
	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);
		struct process *pos = NULL;
		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_BLOCKED);

		list_for_each_entry(pos, &r->waitqueue, list) {
			if(waiter->prio < pos->prio) {
				waiter = pos;
			}
		}

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);


		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/
struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.release = pcp_release,
	.acquire = pcp_acquire,
	.schedule = prio_schedule, // 스케줄링은 우선순위로 해주면 됨
};

static bool pip_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}
	// prio를 젤 높은 애로 줘야 하니까 이거로
	if(r->owner->prio < current->prio) {
		r->owner->prio = current->prio;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_BLOCKED;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

static void pip_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	//prio 복구
	r->owner->prio = r->owner->prio_orig;
	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);
		struct process *pos = NULL;
		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_BLOCKED);

		list_for_each_entry(pos, &r->waitqueue, list) {
			if(waiter->prio < pos->prio) {
				waiter = pos;
			}
		}

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);


		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}
/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.release = pip_release,
	.acquire = pip_acquire,
	.schedule = prio_schedule, // 스케줄링은 그냥 우선순위로 해주면 됨
};
