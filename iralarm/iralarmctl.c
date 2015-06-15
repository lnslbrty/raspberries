#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/sem.h>

#include "irshmem.h"
#include "config.h"


enum ctlopt {
  NONE, GET, SET, ALL, SHELL
};

static const char *irshm_desc[] = { IRSM_DESC };
static const size_t irshm_siz = sizeof(irshm_desc)/sizeof(irshm_desc[0]);

static int sem_id;
static struct sembuf sops[2] = { { 0, -1, 0 }, { 0,  1, 0 } };


static void
usage(char *arg0)
{
  fprintf(stderr, "\nAuthor: Toni Uhlig (matzeton@googlemail.com)\n%s [-a] [-g ID] [-s ID -v VALUE]\n\n"
                  "\t-a\tshow all memory objects\n"
                  "\t-g\tget a var with an id\n"
                  "\t-s\tset a var with an id and value\n"
                  "\t-v\tvalue, used in conjunction with -s\n"
                  "\t-e\tshell mode\n\n", arg0);
}

static void
do_get(int id, void *ptr)
{
  enum shm_object type = irmem_type(id);

  semop(sem_id, &sops[0], 1);
  printf("%s = ", irshm_desc[id]);
  switch (type) {
    case BYTE:
      printf("%u", *(unsigned char *) ptr);
      break;
    case INT16:
      printf("%" PRIu16, *(uint16_t *) ptr);
      break;
    case INT32:
      printf("%" PRIu32, *(uint32_t *) ptr);
      break;
    case INT64:
      printf("%" PRIu64, *(uint64_t *) ptr);
      break;
  }
  printf("\n");
  semop(sem_id, &sops[1], 1);
}

static void
do_all(void)
{
  long unsigned int *ptr;
  int i;

  for (i = 0; i < irshm_siz; i++) {
    ptr = irmem_getptr(i);
    printf("[%d] ", i);
    do_get(i, ptr);
  }
}

static void
do_set(int id, void *ptr, void *valuep)
{
  enum shm_object type = irmem_type(id);

  semop(sem_id, &sops[0], 1);
  printf("%s = ", irshm_desc[id]);
  switch (type) {
    case BYTE:
      *(unsigned char *) ptr = *(unsigned char *) valuep;
      printf("%u" , *(unsigned char *) ptr);
      break;
    case INT16:
      *(uint16_t *) ptr = *(uint16_t *) valuep;
      printf("%" PRIu16, *(uint16_t *) ptr);
      break;
    case INT32:
      *(uint32_t *) ptr = *(uint32_t *) valuep;
      printf("%" PRIu32, *(uint32_t *) ptr);
      break;
    case INT64:
      *(uint64_t *) ptr = *(uint64_t *) valuep;
      printf("%" PRIu64, *(uint64_t *) ptr);
      break;
  }
  printf("\n");
  semop(sem_id, &sops[1], 1);
}

#ifdef _GNU_SOURCE
#define ir_strstr(b, s) strcasestr(b, s)
#else
#define ir_strstr(b, s) strstr(b, s)
#endif
static void
do_shell(void)
{
  char buf[17];
  ssize_t nread;

  setbuf(stdout, NULL);
  printf("> ");
  while ( (nread = read(0, (char *) buf, 16)) > 0 ) {
    buf[16] = '\0';
    if ( ir_strstr(buf, "GET\n") == buf ) {
    } else if ( ir_strstr(buf, "SET\n") == buf ) {
    } else if ( ir_strstr(buf, "SHOW\n") == buf ) {
      do_all();
    } else if ( ir_strstr(buf, "HELP") == buf ) {
      printf("GET, SET, SHOW, HELP\n");
    } else {
      printf("UNKNOWN CMD\n");
    }
    printf("> ");
  }
}

int
str2int(char *buf, unsigned long int *dst)
{
  char *tmp;
  unsigned long int val;

  val = strtoul(buf, &tmp, 10);
  if (buf == tmp) {
    return 1;
  }
  *dst = val;
  return 0;
}

int
main(int argc, char **argv)
{
  int ird_pid;
  int ret, shm_rdonly = 0;
  int opt;
  int value_set = 0, value_inval = 0;
  unsigned long int shmi = 0, value = 0;
  enum ctlopt copt = NONE;

  if (argc == 1) {
    usage(argv[0]);
    exit(1);
  }

  while ((opt = getopt(argc, argv, "ag:s:v:e")) != -1) {
    switch (opt) {
      case 'a':
        copt = ALL;
        break;
      case 'g':
        copt = GET;
        value_inval |= str2int(optarg, &shmi);
        break;
      case 's':
        copt = SET;
        value_inval |= str2int(optarg, &shmi);
        break;
      case 'v':
        value_inval |= str2int(optarg, &value);
        value_set = 1;
        break;
      case 'e':
        copt = SHELL;
        break;
      default:
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if ( value_inval != 0 ) {
    fprintf(stderr, "%s: argument is not numeric\n", argv[0]);
    exit(1);
  }

  sem_id = semget(SEM_ID, 1, IPC_CREAT | IPC_EXCL);
  if (errno != EEXIST) {
    fprintf(stderr, "%s: semaphore key 0x%X does not exist, iralarmd started?\n", argv[0], SEM_ID);
    semctl(sem_id, 1, IPC_RMID, 0);
    exit(1);
  }
  sem_id = semget(SEM_ID, 1, 0);
  ird_pid = semctl(sem_id, 0, GETPID, 0);

  if ( (ret = irmem_init(0)) == IRSM_NINIT) {
    printf("irmem_init() failed\n");
    exit(1);
  }
  if (ret == IRSM_RDONLY) {
    shm_rdonly = 1;
  }

  if (irmem_exists() == IRSM_NEXIST) {
    fprintf(stderr, "%s: shared memory segment doesnt exist (daemon not started?/insufficient right?)\n", argv[0]);
    exit(1);
  }

  void *ptr;
  if ( (ptr = irmem_getptr(shmi)) == NULL ) {
    fprintf(stderr, "%s: irmem_getptr(ID) failed\n", argv[0]);
  } else if (copt == ALL) {
    do_all();
  } else if (copt == GET) {
    do_get(shmi, ptr);
  } else if (copt == SET) {
    if (shm_rdonly == 1) {
      fprintf(stderr, "%s: can not write to shared memory segment (readonly)\n", argv[0]);
      exit(1);
    }
    if (value_set == 0) {
      fprintf(stderr, "%s: -s set but -v not\n", argv[0]);
    } else {
      do_set(shmi, ptr, &value);
      semop(sem_id, &sops[0], 1);
      if (kill(ird_pid, SIGHUP) != 0) {
        fprintf(stderr, "%s: Could not SIGHUP %s: %s\n", argv[0], APPNAME, strerror(errno));
      }
      semop(sem_id, &sops[1], 1);
    }
  } else if (copt == SHELL) {
    do_shell();
  }
  irmem_free(0);
  return 0;
}
