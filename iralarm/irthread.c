#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include "irthread.h"


struct irthread {
  unsigned char actv : 1;
  unsigned char used : 1;
  unsigned char susp : 1;
  pthread_t tid;
  irthread_t id;
  pthread_mutex_t mtx_bsy;
  pthread_cond_t cnd_bsy;
  thread_cb tcb;
  void *data;
};

#define MAX_THREADS 4
struct irthread *thrds[MAX_THREADS];
static pthread_mutex_t tbusy = PTHREAD_MUTEX_INITIALIZER;


static void *
__thread_mainloop(void *arg)
{
  struct irthread *it = (struct irthread *) arg;

  if (it == NULL) return NULL;
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  while (it->actv == 1) {
    pthread_testcancel();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock( &(it->mtx_bsy) );
    if (it->susp == 1) {
      pthread_cond_wait( &(it->cnd_bsy), &(it->mtx_bsy) );
    }
    void *data_ptr = it->data;
    pthread_mutex_unlock( &(it->mtx_bsy) );
    if (it->actv == 1) {
      it->tcb(it->id, data_ptr);
    }
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  }
  return NULL;
}

static inline int
__idcheck(irthread_t id)
{
  return (id < 0 || id >= MAX_THREADS ? 1 : 0);
}

irthread_t
irthread_start(thread_cb threadfunc)
{
  irthread_t id;
  int i, ret = 0;

  pthread_mutex_lock(&tbusy);
  for (i = 0; i < MAX_THREADS; i++) {
    if (thrds[i] != NULL) {
      if (thrds[i]->used == 0) {
        memset(thrds[i], '\0', sizeof(struct irthread));
        break;
      }
    } else {
      thrds[i] = (struct irthread *) calloc(1, sizeof(struct irthread));
      break;
    }
  }
  if (i < MAX_THREADS) {
    id = (irthread_t) i;
    thrds[id]->used = 1;
    thrds[id]->actv = 1;
    thrds[id]->susp = 0;
    thrds[id]->id = id;
    thrds[id]->tcb = threadfunc;
    thrds[id]->data = NULL;
    ret |= pthread_mutex_init( &(thrds[id]->mtx_bsy), NULL );
    ret |= pthread_cond_init( &(thrds[id]->cnd_bsy), NULL );
    ret |= pthread_create( &(thrds[id]->tid), NULL, __thread_mainloop, (void *)thrds[id] );
  } else {
    ret = IRTHRD_FULL;
  }
  pthread_mutex_unlock(&tbusy);
  return (ret != 0 ? IRTHRD_ERR : id);
}

int
irthread_stop(irthread_t id)
{
  int ret = 0;

  if (__idcheck(id) == 0) {
    pthread_mutex_lock(&tbusy);
    irthread_suspend(id);
    thrds[id]->used = 0;
    thrds[id]->actv = 0;
    ret |= pthread_cond_signal( &(thrds[id]->cnd_bsy) );
    ret |= pthread_cancel(thrds[id]->tid);
    ret |= pthread_join(thrds[id]->tid, NULL);
    ret |= pthread_mutex_destroy( &(thrds[id]->mtx_bsy) );
    ret |= pthread_cond_destroy( &(thrds[id]->cnd_bsy) );
    pthread_mutex_unlock(&tbusy);
  } else ret = IRTHRD_ERR;
  return ret;
}

void
irthread_suspend(irthread_t id)
{
  if (thrds[id]->susp == 1) return;
  pthread_mutex_lock( &(thrds[id]->mtx_bsy) );
  thrds[id]->susp = 1;
  pthread_mutex_unlock( &(thrds[id]->mtx_bsy) );
}

int
irthread_resume(irthread_t id)
{
  int ret;
  if (thrds[id]->susp == 0) return -1;
  thrds[id]->susp = 0;
  ret = pthread_cond_signal( &(thrds[id]->cnd_bsy) );
  return ret;
}

void
irthread_setdata(irthread_t id, void *data)
{
  pthread_mutex_lock( &(thrds[id]->mtx_bsy) );
  thrds[id]->data = data;
  pthread_mutex_unlock( &(thrds[id]->mtx_bsy) );
}

