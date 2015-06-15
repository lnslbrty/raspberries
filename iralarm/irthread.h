#ifndef IRTHREAD_H
#define IRTHREAD_H 1

#include <pthread.h>


#define IRTHRD_OK   0
#define IRTHRD_ERR  (int)(-1)
#define IRTHRD_FULL (int)(-2)


typedef int irthread_t;
typedef void (*thread_cb)(irthread_t, void *);


irthread_t
irthread_start(thread_cb threadfunc);

int
irthread_stop(irthread_t id);

void
irthread_suspend(irthread_t id);

int
irthread_resume(irthread_t id);

void
irthread_setdata(irthread_t id, void *data);

#endif
