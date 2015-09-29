#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/sem.h>
#include <time.h>
#include <pwd.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <wiringPi.h>

#include "config.h"
#include "log.h"
#include "irshmem.h"
#include "irpio.h"
#include "irthread.h"
#include "irxmpp.h"


#define PLCK_STAT 0


enum alrm_state {
  ALRM_NONE = 0, ALRM_PREACTIVE = 1, ALRM_ACTIVE = 2
};

static const char appname[] = APPNAME;
static const char author[] = AUTHOR;
static const char email[] = EMAIL;
static size_t ir_recvd = 0;
static size_t ir_recvd_cycls = 0;
static size_t ir_alrm_cycls = 0;
static enum alrm_state astate = ALRM_NONE;
static time_t ir_alrm_start;
static uint32_t ir_minrecvd = MIN_IRCVD;
static uint32_t ir_minalarm = MIN_ALRMS;
static uint32_t ir_rstalarm = MIN_ALRST;

static unsigned char s_aactive = 1, s_speaker = 0, *s_astate;
#ifdef DO_XMPP
static unsigned char s_xmpp = 1;
#endif
#ifdef ENABLE_RASPICAM
static unsigned char s_video = 0;
#endif
static uint32_t s_mainloop = MAINLOOP_SLEEP_DEFAULT, s_statusled_on = 50000, s_statusled_off = 1000000;

static irthread_t thrd_alarm, thrd_status;
static int sem_id;
static struct sembuf sops[2] = { { 0, -1, 0 }, { 0,  1, 0 } };
static sigset_t block_mask;

static pthread_mutex_t mtx_status = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_alarm = PTHREAD_MUTEX_INITIALIZER;

static char *chuser = NULL;


void system_nowait(char *cmd[])
{
  char homedir[PATH_MAX+1];
  char *envp;
  pid_t pid = fork();

  memset(homedir, '\0', PATH_MAX+1);
  snprintf(homedir, PATH_MAX, "%s", ( (envp = getenv("HOME")) != NULL ? envp : "/tmp" ));
  signal(SIGCHLD, SIG_IGN);
  if (pid == 0) {
    close(0);
#ifdef NO_DEBUG
    close(1);
    close(2);
#endif
    clearenv();
    setenv("HOME", homedir, 1);
    execv(cmd[0], cmd);
    _exit(0);
  }
}

static void ir_flank(void)
{
  semop(sem_id, &sops[0], 1);
  ir_recvd++;
  semop(sem_id, &sops[1], 1);
#ifndef NO_DEBUG
  printf(" [0] ");
#endif
}

static inline int
diffTimeWithNow(time_t start, int difft) {
  time_t now;

  time(&now);
  if ( (int) difftime(now, start) > difft ) {
    return 1;
  } else return 0;
}

static inline void
doAlarmAction(void)
{
  struct tm tm;

  tm = *localtime(&ir_alrm_start);
  log_emerg("%04d-%02d-%02d %02d:%02d:%02d ALARM! %d/%d IR-Low-Flanks, %d IR-Recvd-Cycles, %d Alarm-Cycles\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ir_recvd, ir_minrecvd, ir_recvd_cycls, ir_alrm_cycls);
#ifdef DO_XMPP /* TODO: use the faster libstrophe instead of a very slow perl script aka sendxmpp */
  if (s_xmpp > 0) {
    char buf[256];
    memset(buf, '\0', 256);
    snprintf(buf, 255, "ALARM: %04d-%02d-%02d %02d:%02d:%02d!\n%d/%d IR-Low-Flanks, %d IR-Recvd-Cycles, %d Alarm-Cycles", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ir_recvd, ir_minrecvd, ir_recvd_cycls, ir_alrm_cycls);
    irxmpp_sendmsg_trusted(buf);
  }
#endif
#ifdef ENABLE_RASPICAM /* temporary solution */
  if (s_video > 0) {
    char *cmd[] = { "/bin/sh", "-c", "/opt/vc/bin/raspivid -t 60000 -o /run/shm/iralarmd.h264 -w 640 -h 480 -fps 25 -b 1200000 -p 0,0,640,480 && /bin/cp /run/shm/iralarmd.h264 /home/pi/raspicam/iralarmd_$(/bin/date +%d-%m-%y_%H-%M-%S).h264", NULL };
    system_nowait(cmd);
  }
#endif
}

static inline void
undoAlarmAction(void)
{
#ifdef NO_DEBUG
  time_t ir_alrm_stop;
  double difftm;
  struct tm tm;

  time(&ir_alrm_stop);
  difftm = difftime(ir_alrm_stop, ir_alrm_start);
  tm = *localtime(&ir_alrm_stop);
  log_emerg("%04d-%02d-%02d %02d:%02d:%02d ALARM STOPPED, ACTIVE FOR %ds, %d/%d IR-Low-Flanks, %d IR-Recvd-Cycles, %d Alarm-Cycles\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (int) difftm, ir_recvd, ir_rstalarm, ir_recvd_cycls, ir_alrm_cycls);
#endif
}

static void daemonize(char *user)
{
  pid_t pid;
#ifdef NO_DEBUG
  pid = fork();
#else
  pid = 0;
#endif

  if (pid == 0)
  {
    /* child process */
#ifdef NO_DEBUG
    if (setsid() < 0) exit(2);
#endif
    if (user != NULL) {
      clearenv();
      setenv("HOME", "/tmp", 1);
    }
    chdir("/");
#ifdef NO_DEBG
    close(0);
    close(1);
#endif
    setpriority(PRIO_PROCESS, 0, -5);
    if (user != NULL) {
      struct passwd *pwd = getpwnam(user);
      if (pwd != NULL) {
        if (setgid(pwd->pw_gid) != 0 || setuid(pwd->pw_uid) != 0 || setreuid(getuid(), getuid()) != 0) {
          fprintf(stderr, "setuid, setgid, setreuid failed with %d: %s\n", errno, strerror(errno));
          exit(2);
        }
        if (access(pwd->pw_dir, R_OK | W_OK) == 0) {
          setenv("HOME", pwd->pw_dir, 1);
        } else fprintf(stderr, "homedir '%s' does not exist or is not read-/write-able\n", pwd->pw_dir);
      } else {
        fprintf(stderr, "user '%s' not found\n", user);
        exit(2);
      }
    }
#ifdef NO_DEBUG
    close(2);
#endif
  } else if (pid < 0) {
    /* error */
    exit(1);
  } else {
    /* parent */
    exit(0);
  }
}

void
status_led(irthread_t tid, void *data)
{
  pthread_mutex_lock(&mtx_status);
  uint32_t __s_statusled_on = s_statusled_on;
  uint32_t __s_statusled_off = s_statusled_off;
  pthread_mutex_unlock(&mtx_status);

  switch ( (enum alrm_state) data ) {
    case ALRM_NONE:
      digitalWrite(PIN(STATS), 1);
      usleep(__s_statusled_on);
      digitalWrite(PIN(STATS), 0);
      usleep(__s_statusled_off);
      break;
    case ALRM_PREACTIVE:
      digitalWrite(PIN(STATS), 1);
      usleep(__s_statusled_on);
      digitalWrite(PIN(STATS), 0);
      usleep(__s_statusled_on);
      digitalWrite(PIN(STATS), 1);
      usleep(__s_statusled_on);
      digitalWrite(PIN(STATS), 0);
      usleep(__s_statusled_off/2);
      break;
    case ALRM_ACTIVE:
      digitalWrite(PIN(STATS), 1);
      usleep(__s_statusled_on);
      digitalWrite(PIN(STATS), 0);
      usleep(__s_statusled_on*2);
      break;
  }
}

void
alarm_thread(irthread_t tid, void *data)
{
  pthread_mutex_lock(&mtx_alarm);
  uint8_t __s_speaker = s_speaker;
  pthread_mutex_unlock(&mtx_alarm);

  switch ( (enum alrm_state) data ) {
    case ALRM_ACTIVE:
      if (__s_speaker > 0) digitalWrite(PIN(SPEKR), 1);
      digitalWrite(PIN(ALARM), 0);
      usleep(250000);
      digitalWrite(PIN(SPEKR), 0);
      digitalWrite(PIN(ALARM), 1);
      usleep(250000);
      break;
    case ALRM_PREACTIVE:
      digitalWrite(PIN(ALARM), 1);
      usleep(100000);
      digitalWrite(PIN(ALARM), 0);
      usleep(100000);
      break;
    case ALRM_NONE:
      digitalWrite(PIN(ALARM), 0);
      digitalWrite(PIN(SPEKR), 0);
      usleep(100000);
      irthread_suspend(thrd_alarm);
      break;
  }
}

void
irxmpp_thrd_callback(int id) {
  char *buf;
  int ret;
  struct tm tm;

  switch (id) {
    case IRXMPP_STATUS:
      semop(sem_id, &sops[0], 1);
      tm = *localtime(&ir_alrm_start);
      ret = asprintf(&buf, "\n[STATUS]\n"
                           "state.........: %d\n"
                           "recvd.........: %lu\n"
                           "recvd_cycls...: %lu\n"
                           "alarm_cycls...: %lu\n"
                           "last_alarm....: %04d-%02d-%02d %02d:%02d:%02d\n"
                         , astate, (long unsigned int) ir_recvd
                                 , (long unsigned int) ir_recvd_cycls
                                 , (long unsigned int) ir_alrm_cycls
                                 , tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
      if (ret >= 0) {
        irxmpp_sendmsg_trusted(buf);
        free(buf);
      }
      semop(sem_id, &sops[1], 1);
      break;
    case IRXMPP_CONFIG:
      pthread_mutex_lock(&mtx_status);
      pthread_mutex_lock(&mtx_alarm);
      ret = asprintf(&buf, "\n[CONFIG]\n"
                           "active........: %u\n"
                           "mainloop......: %lu\n"
                           "speaker.......: %u\n"
                           "xmpp..........: %u\n"
                           "video.........: %u\n"
                           "statusled-on..: %lu\n"
                           "statusled-off.: %lu\n"
                           "min-recvd.....: %lu\n"
                           "min-alarms....: %lu\n"
                           "rst-alarms....: %lu\n"
                         , s_aactive, (long unsigned int) s_mainloop, s_speaker, s_xmpp, s_video
                         , (long unsigned int) s_statusled_on, (long unsigned int) s_statusled_off
                         , (long unsigned int) ir_minrecvd, (long unsigned int) ir_minalarm, (long unsigned int) ir_rstalarm);
      if (ret >= 0) {
        irxmpp_sendmsg_trusted(buf);
        free(buf);
      }
      pthread_mutex_unlock(&mtx_status);
      pthread_mutex_unlock(&mtx_alarm);
      break;
  }
}

static void sighandler(int signum)
{
  uint32_t *szptr;
  unsigned char *ucptr;

  sigprocmask(SIG_BLOCK, &block_mask, NULL);
  log("got signal %d", signum);
  switch (signum) {
    case SIGKILL:
      log("%s", "something want to kill me");
      break;
    case SIGSEGV:
      log("%s", "segmentation fault");
      break;
    case SIGALRM:
      break;
    case SIGTERM:
    case SIGINT:
      irxmpp_stopThread();
      irthread_stop(thrd_alarm);
      irthread_stop(thrd_status);
      irpio_lowall();
      irmem_free(1);
      semctl(sem_id, 1, IPC_RMID, 0);
      log_close();
      exit(0);
      break;
    case SIGHUP:
      pthread_mutex_lock(&mtx_status);
      pthread_mutex_lock(&mtx_alarm);
      GETPTR_SAFE(SHM_ACTIVE, s_aactive, ucptr);
      GETPTR_SAFE(SHM_MLOOP, s_mainloop, szptr);
      GETPTR_SAFE(SHM_SPKR, s_speaker, ucptr);
      GETPTR_SAFE(SHM_XMPP, s_xmpp, ucptr);
      GETPTR_SAFE(SHM_VIDEO, s_video, ucptr);
      GETPTR_SAFE(SHM_STLEDON, s_statusled_on, szptr);
      GETPTR_SAFE(SHM_STLEDOFF, s_statusled_off, szptr);
      GETPTR_SAFE(SHM_MINRECVD, ir_minrecvd, szptr);
      GETPTR_SAFE(SHM_MINALRMS, ir_minalarm, szptr);
      GETPTR_SAFE(SHM_RSTALRMS, ir_rstalarm, szptr);
      pthread_mutex_unlock(&mtx_alarm);
      pthread_mutex_unlock(&mtx_status);
      break;
  }
}

static void usage(char *arg0)
{
  static const char usg[] = "%s (C) by %s (bugreports to %s)\n\n"
                            "%s [ARGS]\n"
                            "where [ARGS] can be:\n"
                            "\t-u [USER]\tchange user\n"
#ifdef DO_XMPP
                            "\t-x [JID]\ttrusted jabber id\n"
                            "\t-j [JID]\tlogin jabber id\n"
                            "\t-s [HOST]\tjabber server\n"
                            "\t-p [PORT]\tspecify an alternative jabber port\n"
#endif
                            "\n";
  fprintf(stderr, usg, appname, author, email, arg0);
}

int
main(int argc, char **argv)
{
  uint32_t *szptr;
  unsigned char *ucptr;
  int opt;
  char *tmp;
  unsigned long int tmpval;
#ifdef DO_XMPP
  uint8_t xmpp_opts = 0;
#endif


  /* set up signal'ing */
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGHUP);
  sigaddset(&block_mask, SIGALRM);
  sigaddset(&block_mask, SIGTERM);
  sigaddset(&block_mask, SIGINT);
  signal(SIGHUP,  sighandler);
  signal(SIGALRM, sighandler);
  signal(SIGTERM, sighandler);
  signal(SIGINT,  sighandler);
  signal(SIGKILL, sighandler);
  signal(SIGSEGV, sighandler);
  pthread_sigmask(SIG_BLOCK, &block_mask, NULL);
  sigprocmask(SIG_BLOCK, &block_mask, NULL);

  log_init();
  irxmpp_initialize(irxmpp_thrd_callback);

  while ( (opt = getopt(argc, argv, "hu:"
#ifdef DO_XMPP
                                    "x:j:s:p:"
#endif
                                    )) != -1 ) {
    switch (opt) {
      case 'u':
        chuser = strdup(optarg);
        break;
#ifdef DO_XMPP
      /* xmpp trusted jid */
      case 'x':
        irxmpp_set_trusted(optarg);
        xmpp_opts |= 0x1;
        break;
      /* login jid */
      case 'j':
        irxmpp_set_jid(optarg);
        xmpp_opts |= 0x2;
        break;
      /* xmpp server */
      case 's':
        irxmpp_set_host(optarg);
        xmpp_opts |= 0x4;
        break;
      /* xmpp port */
      case 'p':
        tmpval = strtoul(optarg, &tmp, 10);
        if (optarg == tmp) {
          fprintf(stderr, "%s: incorrect port (-p) argument\n", argv[0]);
        } else {
          irxmpp_set_port(tmpval);
        }
        xmpp_opts |= 0x8;
        break;
#endif
      case 'h':
      default:
        usage(argv[0]);
        exit(1);
        break;
    }
  }
#ifdef DO_XMPP
  if (xmpp_opts < 0x7 && xmpp_opts != 0) {
    fprintf(stderr, "%s: missing xmpp arguments (-x , -j , -s , [-p])\n", argv[0]);
    exit(1);
  }
#endif
  if (getuid() != 0) {
    fprintf(stderr, "%s: Only root can access the GPIO interface!\n", argv[0]);
    exit(1);
  }

  wiringPiSetup();
  daemonize(chuser);

#ifdef DO_XMPP
  switch ( irxmpp_check_passwd_file(&tmp) ) {
    case ERR_OK:
      break;
    case ERR_SYSERR:
      log_err("%s: XMPP passwdfile (%s) check failed: %s\n", argv[0], tmp, strerror(errno));
      exit(1);
    case ERR_CFG_MODE:
      log_err("%s: Check XMPP passwd file (%s) failed. It must have mode 0400 or 0600!\n", argv[0], tmp);
      exit(1);
    default:
      log_err("%s: XMPP passwd file check failed\n", argv[0]);
      exit(1);
  }
  free(tmp);

  switch (irxmpp_read_passwd_file()) {
    case ERR_OK:
      break;
    case ERR_SYSERR:
      fprintf(stderr, "%s: Could not read XMPP passwdfile: %s\n", argv[0], strerror(errno));
      exit(1);
    default:
      fprintf(stderr, "%s: XMPP passwd file read failed\n", argv[0]);
      exit(1);
  }

  switch ( irxmpp_startThread() ) {
    case 0:
      break;
    case ERR_NHOST:
      fprintf(stderr, "%s: XMPP init failed (host not set).\n", argv[0]);
      exit(1);
    case ERR_NTRUSTED:
      fprintf(stderr, "%s: XMPP init failed (trusted jid not set).\n", argv[0]);
      exit(1);
    case ERR_NPASS:
      fprintf(stderr, "%s: XMPP init failed (password not set).\n", argv[0]);
      exit(1);
    case ERR_NJID:
      fprintf(stderr, "%s: XMPP init failed (jid not set).\n", argv[0]);
      exit(1);
    default:
      fprintf(stderr, "%s: XMPP init failed (Thread error)\n", argv[0]);
      exit(1);
  }
  irxmpp_thread_connect_wait();
#endif

  /* initialise shared memory segments (e.g. used by iralarmctl) */
  if (irmem_init(1) != IRSM_OK) {
    fprintf(stderr, "%s: Could not init shared memory sgements.\n", argv[0]);
    exit(1);
  }

  /* set shared memory objects to a default value */
  SETPTR_SAFE(SHM_ACTIVE, s_aactive, ucptr);
  SETPTR_SAFE(SHM_MLOOP, s_mainloop, szptr);
  SETPTR_SAFE(SHM_SPKR, s_speaker, ucptr);
  SETPTR_SAFE(SHM_XMPP, s_xmpp, ucptr);
  SETPTR_SAFE(SHM_VIDEO, s_video, ucptr);
  SETPTR_SAFE(SHM_STLEDON, s_statusled_on, szptr);
  SETPTR_SAFE(SHM_STLEDOFF, s_statusled_off, szptr);
  SETPTR_SAFE(SHM_MINRECVD, ir_minrecvd, szptr);
  SETPTR_SAFE(SHM_MINALRMS, ir_minalarm, szptr);
  SETPTR_SAFE(SHM_RSTALRMS, ir_rstalarm, szptr);
  SETPTR_SAFE(SHM_ALARM, astate, s_astate);

  /* init raspberrypi GPIO's */
  irpio_init();
  irpio_setpwm(RPIO_PWM_CLOCK, RPIO_PWM_RANGE);
#ifdef NO_DEBUG
  irpio_selftest(); // selftest is not useful
#endif
  irpio_setvalue(IRLED, RPIO_PWM_RANGE-1);
  thrd_status = irthread_start(status_led);
  thrd_alarm = irthread_start(alarm_thread);
  irthread_suspend(thrd_alarm);
  wiringPiISR(PIN(IRECV), INT_EDGE_FALLING, ir_flank);

  /* set up WAIT semaphore */
  if ( (sem_id = semget(SEM_ID, 1, IPC_CREAT | 0660)) == -1 ) {
    log_ret("semaphore initialisation");
    sigprocmask(SIG_UNBLOCK, &block_mask, NULL);
    raise(SIGTERM);
  }
  semop(sem_id, &sops[1], 1);

  for (;;)
  {
    sigprocmask(SIG_UNBLOCK, &block_mask, NULL);
    usleep(s_mainloop);
    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    semop(sem_id, &sops[0], 1);
#ifndef NO_DEBUG
    printf("\t%d\n", ir_recvd);
#endif

    if (ir_recvd < ir_minrecvd) {
      ir_alrm_cycls++;
    } else {
      ir_recvd_cycls++;
      if (ir_alrm_cycls > 0) ir_alrm_cycls--;
    }

    *s_astate = astate;
    switch (astate) {
      case ALRM_NONE:
        if ( ir_alrm_cycls > ir_minalarm ) {
          time(&ir_alrm_start);
          doAlarmAction();
          ir_recvd_cycls = 0;
          astate = ALRM_PREACTIVE;
          irthread_setdata(thrd_alarm, (void *) astate);
          irthread_setdata(thrd_status, (void *) astate);
          irthread_resume(thrd_alarm);
        }
        break;
      case ALRM_PREACTIVE:
        if ( (int) diffTimeWithNow(ir_alrm_start, 10) == 1 ) {
          ir_recvd_cycls = 0;
          astate = ALRM_ACTIVE;
          irthread_setdata(thrd_alarm, (void *) astate);
          irthread_setdata(thrd_status, (void *) astate);
        } else if (ir_recvd_cycls > ir_rstalarm) {
          astate = ALRM_NONE;
          irthread_setdata(thrd_alarm, (void *) astate);
          irthread_setdata(thrd_status, (void *) astate);
          undoAlarmAction();
          ir_alrm_cycls = 0;
        }
        break;
      case ALRM_ACTIVE:
        if ( ir_recvd_cycls > ir_rstalarm ) {
          astate = ALRM_NONE;
          irthread_setdata(thrd_alarm, (void *) astate);
          irthread_setdata(thrd_status, (void *) astate);
          undoAlarmAction();
          ir_alrm_cycls = 0;
        }
        break;
    }
    ir_recvd = 0;
    semop(sem_id, &sops[1], 1);
  }
  raise(SIGTERM);
  return 0;
}
