#ifndef IRXMPP_H
#define IRXMPP_H 1

#define ERR_OK       (int)( 0)
#define ERR_NHOST    (int)(-1)
#define ERR_NJID     (int)(-2)
#define ERR_NPASS    (int)(-3)
#define ERR_NTRUSTED (int)(-4)
#define ERR_CFG_MODE (int)(-5)
#define ERR_SYSERR   (int)(-6)
#define ERR_PATH     (int)(-7)
#define ERR_UNKWNCMD (int)(-8)

typedef void (*irxmpp_cb)(int);


extern const char *passwd_file;

void
irxmpp_set_jid(char *jid);

void
irxmpp_set_trusted(char *jid);

void
irxmpp_set_pass(char *pass);

int
irxmpp_read_passwd_file(void);

int
irxmpp_check_passwd_file(char **p_path);

void
irxmpp_set_host(char *new_host);

void
irxmpp_set_port(unsigned short new_port);

void
irxmpp_sendmsg(char *to, char *msg);

inline void
irxmpp_sendmsg_trusted(char *msg);

void
irxmpp_initialize(irxmpp_cb __cmd_cb);

void
irxmpp_stopThread(void);

int inline
irxmpp_startThread(void);

void
irxmpp_thread_connect_wait(void);

#endif
