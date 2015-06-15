#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <pthread.h>
#include <strophe.h>

#include "config.h"
#include "log.h"
#include "irxmpp.h"


const char *passwd_file = ".irxmppasswd";

static char *trusted_jid = NULL;
static char *host = NULL;
static unsigned short port = 0;
static xmpp_ctx_t *ctx = NULL;
static xmpp_conn_t *conn = NULL;
static xmpp_log_t log;
static unsigned char do_reconnect = 0;
#ifdef NO_DEBUG
static xmpp_log_level_t loglvl = XMPP_LEVEL_DEBUG;
#else
static xmpp_log_level_t loglvl = XMPP_LEVEL_DEBUG;
#endif

static pthread_t thrd;
static pthread_mutex_t thrd_conn_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thrd_conn_cond = PTHREAD_COND_INITIALIZER;

struct irxmpp_cb {
  const char *cmd;
  int id;
  const char *desc;
};

static struct irxmpp_cb irxmpp_cb_args[] = { IRXMPP_CBARGS };
static irxmpp_cb cmd_cb;


void
irxmpp_set_jid(char *jid)
{
  if (conn != NULL) {
    xmpp_conn_set_jid(conn, jid);
  }
}

void
irxmpp_set_trusted(char *jid)
{
  if (jid != NULL) {
    if (trusted_jid != NULL) {
      free(trusted_jid);
    }
    trusted_jid = strndup(jid, BUFSIZ);
  }
}

void
irxmpp_set_pass(char *pass)
{
  if (conn != NULL) {
    xmpp_conn_set_pass(conn, pass);
  }
}

static char *
__build_passwd_path(void)
{
  char *home = getenv("HOME");
  if (home != NULL) {
    size_t hlen = strnlen(home, PATH_MAX);
    size_t plen = strnlen(passwd_file, PATH_MAX);
    char *retstr = calloc(hlen + plen + 2, sizeof(char));
    strncat(retstr, home, hlen);
    if ( *(retstr + hlen - 1) != '/' ) {
      *(retstr + hlen) = '/';
    }
    strncat(retstr, passwd_file, plen);
    return retstr;
  }
  return NULL;
}

int
irxmpp_read_passwd_file(void)
{
  char *path = __build_passwd_path();
  char passwd[BUFSIZ], *tmp;
  FILE *file;
  int retval = ERR_OK;

  errno = 0;
  if (path != NULL) {
    if ( (file = fopen(path, "r")) != NULL ) {
      memset(passwd, '\0', BUFSIZ);
      if ( fread( (char *) passwd, 1, BUFSIZ - 1, file ) >= 0 ) {
        tmp = strstr( (char *) passwd, "\n");
        if (tmp != NULL) {
          *tmp = '\0';
        }
        irxmpp_set_pass( (char *) passwd );
      } else retval = ERR_SYSERR;
      fclose(file);
    } else retval = ERR_SYSERR;
    free(path);
  } else retval = ERR_PATH;
  return retval;
}

int 
irxmpp_check_passwd_file(char **p_path)
{
  char *path = __build_passwd_path();   
  struct stat st;
  int retval = ERR_OK, i;
  static const int frbddn_bits[] = { S_ISUID, S_ISGID, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH };

  errno = 0;
  if (path != NULL) {
    if ( stat(path, &st) == 0 ) {
      if ( S_ISREG(st.st_mode) && (st.st_mode & S_IRUSR) ) {
        for (i = 0; i < sizeof(frbddn_bits)/sizeof(frbddn_bits[0]); i++) {
          if ( (st.st_mode & frbddn_bits[i]) != 0) {
            retval = ERR_CFG_MODE;
            break;
          }
        }
      } else retval = ERR_CFG_MODE;
    } else retval = ERR_SYSERR;
    if (p_path != NULL) {
      *p_path = path;
    } else {
      free(path);
    }
  } else retval = ERR_PATH;
  return retval;
}

void
irxmpp_set_host(char *new_host)
{
  if (new_host != NULL) {
    if (host != NULL) {
      free(host);
    }
    host = strndup(new_host, BUFSIZ);
  }
}

void
irxmpp_set_port(unsigned short new_port)
{
  port = new_port;
}


void
__irxmpp_log_handler(void *const userdata, const xmpp_log_level_t lvl, const char *const area, const char *const msg)
{
  xmpp_log_level_t filter = * (xmpp_log_level_t *) userdata;
  if (lvl >= filter) {
    log("irxmpp(%s): %s", area, msg);
  }
}

static void
__irxmpp_default_iq(xmpp_stanza_t * const stanza, xmpp_stanza_t **reply, xmpp_stanza_t **query, const char *query_root)
{
  char *ns, *from;

  *reply = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(*reply, "iq");
  xmpp_stanza_set_type(*reply, "result");
  xmpp_stanza_set_id(*reply, xmpp_stanza_get_id(stanza));
  if ( (from = xmpp_stanza_get_attribute(stanza, "from")) != NULL ) {
    xmpp_stanza_set_attribute(*reply, "to", from);
  }
  *query = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(*query, query_root);
  ns = xmpp_stanza_get_ns(xmpp_stanza_get_children(stanza));
  if (ns) {
    xmpp_stanza_set_ns(*query, ns);
  }
}

static int
__irxmpp_version_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
  xmpp_stanza_t *reply, *query, *name, *version, *text;

  __irxmpp_default_iq(stanza, &reply, &query, "query");
  name = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(name, "name");
  /* version text stanza */
  text = xmpp_stanza_new(ctx);
  xmpp_stanza_set_text(text, APPNAME);
  xmpp_stanza_add_child(name, text);
  xmpp_stanza_release(text);
  xmpp_stanza_add_child(query, name);
  xmpp_stanza_release(name);
  /* author text stanza */
  version = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(version, "author");
  xmpp_stanza_add_child(query, version);
  xmpp_stanza_release(version);
  text = xmpp_stanza_new(ctx);
  xmpp_stanza_set_text(text, AUTHOR);
  xmpp_stanza_add_child(version, text);
  xmpp_stanza_release(text);
  xmpp_stanza_add_child(reply, query);
  xmpp_stanza_release(query);
  xmpp_send(conn, reply);
  xmpp_stanza_release(reply);
  return 1;
}

static int
__irxmpp_time_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
  xmpp_stanza_t *reply, *query;

  __irxmpp_default_iq(stanza, &reply, &query, "time");
  xmpp_stanza_set_attribute(reply, "seconds", "0");
  xmpp_stanza_add_child(reply, query);
  xmpp_stanza_release(query);
  xmpp_send(conn, reply);
  xmpp_stanza_release(reply);
  return 1;
}

static int
__irxmpp_last_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
  xmpp_stanza_t *reply, *query, *name, *version, *text;
  xmpp_ctx_t *ctx = (xmpp_ctx_t*) userdata;

  __irxmpp_default_iq(stanza, &reply, &query, "time");
  name = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(name, "tzo");
  text = xmpp_stanza_new(ctx);
  xmpp_stanza_set_text(text, "");
  xmpp_stanza_add_child(name, text);
  xmpp_stanza_release(text);
  xmpp_stanza_add_child(query, name);
  xmpp_stanza_release(name);
  version = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(version, "utc");
  xmpp_stanza_add_child(query, version);
  xmpp_stanza_release(version);
  text = xmpp_stanza_new(ctx);
  xmpp_stanza_set_text(text, "");
  xmpp_stanza_add_child(version, text);
  xmpp_stanza_release(text);
  xmpp_stanza_add_child(reply, query);
  xmpp_stanza_release(query);
  xmpp_send(conn, reply);
  xmpp_stanza_release(reply);
  return 1;
}

void
irxmpp_sendmsg(char *to, char *msg)
{
  xmpp_stanza_t *reply = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(reply, "message");
  xmpp_stanza_set_type(reply, "chat");
  xmpp_stanza_set_attribute(reply, "to", to);

  xmpp_stanza_t *body = xmpp_stanza_new(ctx);
  xmpp_stanza_set_name(body, "body");

  xmpp_stanza_t *text = xmpp_stanza_new(ctx);
  xmpp_stanza_set_text(text, msg);

  xmpp_stanza_add_child(body, text);
  xmpp_stanza_release(text);
  xmpp_stanza_add_child(reply, body);
  xmpp_stanza_release(body);
  xmpp_send(conn, reply);
  xmpp_stanza_release(reply);
}

inline void
irxmpp_sendmsg_trusted(char *msg)
{
  irxmpp_sendmsg(trusted_jid, msg);
}

static char *
__sprint_commands(void)
{
  int i;
  size_t sz_cur = 1, sz_max = BUFSIZ, strlen_cmd, strlen_desc;
  char *ret;

  ret = calloc(sz_max, sizeof(char));
  ret[0] = '\n';
  for (i = 0; i < sizeof(irxmpp_cb_args)/sizeof(irxmpp_cb_args[0]); i++) {
    strlen_cmd = strnlen(irxmpp_cb_args[i].cmd, BUFSIZ);
    strlen_desc = strnlen(irxmpp_cb_args[i].desc, BUFSIZ);

    while (strlen_cmd + strlen_desc + sz_cur + 4 >= sz_max) {
      sz_max *= 2;
      ret = realloc(ret, sz_max);
    }

    strncat(ret, irxmpp_cb_args[i].cmd, strlen_cmd);
    strcat(ret, " - "); // +3
    strncat(ret, irxmpp_cb_args[i].desc, strlen_desc);
    strcat(ret, "\n"); // +1
  }
  return ret;
}

static int
__parse_command(char *cmd)
{
  int i;

  for (i = 0; i < sizeof(irxmpp_cb_args)/sizeof(irxmpp_cb_args[0]); i++) {
    if ( strcasestr(cmd, irxmpp_cb_args[i].cmd) == cmd ) {
      return irxmpp_cb_args[i].id;
    }
  }
  return ERR_UNKWNCMD;
}

static int
__irxmpp_message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
  char *from = xmpp_stanza_get_attribute(stanza, "from");
  xmpp_stanza_t *body = xmpp_stanza_get_child_by_name(stanza, "body");
  static char *msg, buf[BUFSIZ];
  int cb_id;

  if (from == NULL || body == NULL) {
    return 1;
  }
  if ( strstr(from, trusted_jid) != from ) {
    return 1;
  }
  if ( (msg = xmpp_stanza_get_text(body)) != NULL ) {
    if ( (cb_id = __parse_command(msg)) != ERR_UNKWNCMD ) {
      cmd_cb(cb_id);
    } else if ( strcasestr(msg, "ping" ) == msg ) {
      irxmpp_sendmsg_trusted("PONG!");
    } else if ( strcasestr(msg, "version" ) == msg ) {
      memset(buf, '\0', BUFSIZ);
      snprintf(buf, BUFSIZ, "\n"
                    "Software...: %s\n"
                    "Author.....: %s\n"
                    "Contact....: %s\n"
                  , APPNAME, AUTHOR, EMAIL);
      irxmpp_sendmsg_trusted(buf);
    } else if ( strcasestr(msg, "help" ) == msg ) {
      irxmpp_sendmsg_trusted("I'm able to understand the following commands:\n"
        "ping - pong\n"
        "version - print version and author");
      char *cmds = __sprint_commands();
      irxmpp_sendmsg_trusted(cmds);
      free(cmds);
    } else {
      irxmpp_sendmsg_trusted("I don't understand. Type 'help' for more information.");
    }
    xmpp_free(ctx, msg);
  }
  return 1;
}

static int
__irxmpp_presence_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
  char *from = xmpp_stanza_get_attribute(stanza, "from");
  char *type = xmpp_stanza_get_attribute(stanza, "type");

  if (from == NULL) {
    return 1;
  }
  if ( strstr(from, trusted_jid) != from ) {
    return 1;
  }
  if (type != NULL && strstr(type, "unavailable") == type) {
    return 1;
  }

  if (!xmpp_stanza_get_child_by_name(stanza, "show") && xmpp_stanza_get_attribute(stanza, "to")) {
    irxmpp_sendmsg_trusted("Welcome trusted user.");
  }
  return 1;
}

static int
__irxmpp_ping_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
  if (xmpp_stanza_get_child_by_name(stanza, "ping") != NULL) {
    if (xmpp_stanza_get_attribute(stanza, "type") != NULL && xmpp_stanza_get_attribute(stanza, "id") != NULL) {
      xmpp_stanza_t *reply;
      reply = xmpp_stanza_new(ctx);
      xmpp_stanza_set_name(reply, "iq");
      xmpp_stanza_set_type(reply, "result");
      xmpp_stanza_set_id(reply, xmpp_stanza_get_id(stanza));
      xmpp_send(conn, reply);
      xmpp_stanza_release(reply);
    }
  }
  return 1;
}

static void
__irxmpp_conn_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status,
  const int error, xmpp_stream_error_t * const stream_error,
  void * const userdata)
{
  xmpp_ctx_t *ctx = (xmpp_ctx_t *) userdata;

  if (status == XMPP_CONN_CONNECT) {
    xmpp_stanza_t* pres;
    xmpp_handler_add(conn, __irxmpp_ping_handler, "urn:xmpp:ping", "iq", NULL, ctx);
    xmpp_handler_add(conn, __irxmpp_version_handler, "jabber:iq:version", "iq", NULL, ctx);
    xmpp_handler_add(conn, __irxmpp_last_handler, "jabber:iq:last", "iq", NULL, ctx);
    xmpp_handler_add(conn, __irxmpp_time_handler, "urn:xmpp:time", "iq", NULL, ctx);
    xmpp_handler_add(conn, __irxmpp_message_handler, NULL, "message", NULL, ctx);
    xmpp_handler_add(conn, __irxmpp_presence_handler, NULL, "presence", NULL, ctx);
    pres = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(pres, "presence");
    xmpp_send(conn, pres);
    xmpp_stanza_release(pres);
    pthread_cond_signal(&thrd_conn_cond);
  } else {
    xmpp_stop(ctx);
  }
}

void
irxmpp_initialize(irxmpp_cb __cmd_cb)
{
  cmd_cb = __cmd_cb;
  /* init library */
  xmpp_initialize();
  /* use our logging handler */
  log.handler = __irxmpp_log_handler;
  log.userdata = (void *) &loglvl;
  /* create a context */
  ctx = xmpp_ctx_new(NULL, &log);
  /* create a connection */
  conn = xmpp_conn_new(ctx);
}

static int
__irxmpp_connect(void)
{
  do_reconnect = 1;
  do {
    /* initiate connection */
    if ( xmpp_connect_client(conn, host, port, __irxmpp_conn_handler, ctx) != 0 ) {
      sleep(2);
      continue;
    }
    xmpp_run(ctx);
  } while (do_reconnect != 0);
  pthread_mutex_unlock(&thrd_conn_mtx);
  return 0;
}

void
irxmpp_thread_connect_wait(void)
{
  pthread_cond_wait(&thrd_conn_cond, &thrd_conn_mtx);
}

void
irxmpp_stopThread(void)
{
  int *intp;

  do_reconnect = 0;
  xmpp_disconnect(conn);
  pthread_join(thrd, (void **) &intp);
  /* release our connection and context */
  xmpp_conn_release(conn);
  xmpp_ctx_free(ctx);
  /* final shutdown of the library */
  xmpp_shutdown();
}

static void *
__irxmpp_thrd(void *data)
{
  return ( (int *)( __irxmpp_connect() ) );
}

int inline
irxmpp_startThread(void)
{
  pthread_mutex_lock(&thrd_conn_mtx);
  if (host == NULL) {
    return ERR_NHOST;
  }
  if (xmpp_conn_get_jid(conn) == NULL) {
    return ERR_NJID;
  }
  if (xmpp_conn_get_pass(conn) == NULL) {
    return ERR_NPASS;
  }
  if (trusted_jid == NULL) {
    return ERR_NTRUSTED;;
  }
  return ( pthread_create(&thrd, NULL, __irxmpp_thrd, NULL) );
}

