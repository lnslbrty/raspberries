#ifndef IRSHMEM_H
#define IRSHMEM_H 1

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define IMEM_DEFAULT IPC_CREAT | 0660
#define IMEM_EXISTS  0600
#define IMEM_FALLBCK 0400

#define IRSM_OK     0
#define IRSM_RDONLY (int)(-1)
#define IRSM_NINIT  (int)(-2)
#define IRSM_NEXIST (int)(-3)

#define GETPTR_SAFE(id, dst, tmp) if ( (tmp = irmem_getptr(id)) != NULL ) { dst = *tmp; }
#define SETPTR_SAFE(id, val, tmp) if ( (tmp = irmem_getptr(id)) != NULL ) { *tmp = val; }


#define SHMSEG(siz) { siz }
enum shm_object {
  BYTE = 1, INT16 = 2, INT32 = 4, INT64 = 8
};


typedef int irsm_result;

irsm_result
irmem_init(int create);

inline void *
irmem_getptr(size_t id);

size_t
irmem_type(size_t id);

void
irmem_free(int destroy_shm);

irsm_result
irmem_exists(void);

#endif
