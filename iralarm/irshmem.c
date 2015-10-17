#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>
#include <errno.h>

#include "irshmem.h"
#include "config.h"


struct irshmem_data {
  off_t p_off;
  void *ptr;
};

static struct irshmem_data *shmem;
static int shmkey = -1;
static void *shmptr = NULL;
static size_t shmsiz = 0;
static const enum shm_object initial[] = { IRSM_DATA };

irsm_result
irmem_init(int create)
{
  int i;
  irsm_result ret = IRSM_OK;
  off_t cur_offset = 0;

  shmsiz = sizeof(initial)/sizeof(initial[0]);
  shmem = calloc(shmsiz, sizeof(struct irshmem_data));
  for (i = 0; i < shmsiz; i++) {
    shmem[i].p_off = cur_offset;
    cur_offset += initial[i];
  }
  if ( (shmkey = shmget(SHM_SEGID, cur_offset, (create == 0 ? IMEM_EXISTS : IMEM_DEFAULT))) == -1 ) {
    if ( (shmkey = shmget(SHM_SEGID, cur_offset, IMEM_FALLBCK)) == -1 ) {
      fprintf(stderr, "Error (%d) while get shared memory segment (%lu bytes): %s\n", errno, cur_offset, strerror(errno));
      ret = IRSM_NINIT;
      return ret;
    } else { 
      ret = IRSM_RDONLY;
    }
  }
  if ( (shmptr = shmat(shmkey, NULL, (ret == IRSM_RDONLY ? SHM_RDONLY : 0) )) == (int *)(-1) ) {
    fprintf(stderr, "Error (%d) while attach shared memory segment: %s\n", errno, strerror(errno));
    ret = IRSM_NINIT;
  } else {
    for (i = 0; i < shmsiz; i++) {
      shmem[i].ptr = (void *)((int *) shmptr + shmem[i].p_off);
    }
  }
  return ret;
}

inline void *
irmem_getptr(size_t id)
{
  if (id >= shmsiz) return NULL;
  return shmem[id].ptr;
}

size_t
irmem_type(size_t id)
{
  if (id >= shmsiz) return 0;
  return initial[id];
}

void
irmem_free(int destroy_shm)
{
  if (destroy_shm > 0) {
    shmctl(shmkey, IPC_RMID, NULL);
  }
  free(shmem);
}

irsm_result
irmem_exists(void)
{
  struct shmid_ds buf;

  if (shmkey >= 0 && shmctl(shmkey, IPC_STAT, &buf) == 0) {
    return IRSM_OK;
  }
  return IRSM_NEXIST;
}

