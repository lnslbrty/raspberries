#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "irthread.h"
#include "irshmem.h"
#include "config.h"


void
__thread_func(irthread_t id, void *data)
{
  printf("Thread[%d] active!\n", id);
  sleep(2);
}

int main(int argc, char **argv)
{
  int i;
  irthread_t tid[8]; 

  printf("Thread tests ..\n");
  for (i = 0; i < 8; i++) {
    tid[i] = irthread_start(__thread_func);
    sleep(1);
  }
  printf("Threads running .. suspending 0\n");
  irthread_suspend(tid[0]);
  sleep(8);
  for (i = 0; i < 8; i++) {
    printf("try to stop Thread(%d): %d\n", i, irthread_stop(tid[i]));
  }
  printf("Threads stopped ..\n");

  if (irmem_init(0) != IRSM_OK) {
    printf("irmem_init() failed\n");
    exit(1);
  }
  int *ptr;
  if ( (ptr = irmem_getptr(SHM_MLOOP)) != NULL ) {
    printf("shm_mloop value: %d (%p)\n", *ptr, ptr);
  } else printf("irmem_getptr(SHM_MLOOP) failed\n");
  if ( (ptr = irmem_getptr(SHM_ACTIVE)) != NULL ) {
    printf("shm_active value: %d (%p)\n", *ptr, ptr);
  } else printf("irmem_getptr(SHM_ACTIVE) failed\n");
  system("ipcs -m");
  irmem_free(0);

  return 0;
}
