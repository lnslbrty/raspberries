/* uncomment this for debug mode */
#if defined(_NO_DEBUG) && _NO_DEBUG == 0
#undef NO_DEBUG
#else
#define NO_DEBUG  1
#endif

#define IRALARM_PASSWDFILE "/etc/irxmppasswd"

/* your rpi pinout */
/* element position in the rpio pinout array */
#define RPIO_PINPOS \
	IRLED = 0,	\
	IRECV,		\
	SPEKR,		\
	ALARM,		\
	STATS

/* pinout structure { PIN_NAME, WPI_PIN, WPI_DIRECTION, ENABLED } */
#define RPIO_PINOUT \
	{ "IR-LED",      1, PWM_OUTPUT, 1 },	\
	{ "IR-RECIEVER", 3, INPUT     , 1 },	\
	{ "SPEAKER",     2, OUTPUT    , 1 },	\
	{ "ALARM-LED",   0, OUTPUT    , 1 },	\
	{ "STATUS-LED",  4, OUTPUT    , 1 }

#define IPC_MAX_SHMSEGS 16

/* exported shared memory object IDs for internal use */
#define SHM_MLOOP    0
#define SHM_ACTIVE   1
#define SHM_SPKR     2
#define SHM_XMPP     3
#define SHM_VIDEO    4
#define SHM_STLEDON  5
#define SHM_STLEDOFF 6
#define SHM_MINRECVD 7
#define SHM_MINALRMS 8
#define SHM_RSTALRMS 9
#define SHM_ALARM    10

#define SHM_SEGID 0x1337
#define SEM_ID    0x1338

#define IRSM_DATA \
	/* mloop */   INT32, \
	/* active */  BYTE, \
	/* speaker */ BYTE, \
	/* xmpp */    BYTE, \
	/* video */   BYTE, \
	/* led on */  INT32, \
	/* led off */ INT32, \
	/* minrcvd */ INT32, \
	/* minalrm */ INT32, \
	/* rstalrm */ INT32, \
	/* alarm */   BYTE

/* human readable description of the memory object */
#define IRSM_DESC \
	"MAINLOOP", "ACTIVE", "SPEAKER", "SENDXMPP", "RASPICAM", "STATUSLED-ON", "STATUSLED-OFF", "IR-MINRECVD", "IR-MINALRMS", "IR-RESETALRMS", "ALARM-STATE"

#define MAINLOOP_SLEEP_DEFAULT 100000

/* hardware PWM related */
#define RPIO_PWM_CLOCK 199.15
#define RPIO_PWM_RANGE 1024

/* alarm specs */
#define MIN_IRCVD 8
#define MIN_ALRMS 2
#define MIN_ALRST 15

/* send a XMPP message if in alarm state */
#define DO_XMPP 1
#define IRXMPP_STATUS 0
#define IRXMPP_CONFIG 1
#define IRXMPP_CBARGS \
	{ "status", IRXMPP_STATUS, "print alarm information" }, \
	{ "config", IRXMPP_CONFIG, "print configuration values" }
/* shot some pic's if alarm is active */
#define ENABLE_RASPICAM 1

#define APPNAME   "iralarmd"
#define AUTHOR    "Toni Uhlig"
#define EMAIL     "matzeton@googlemail.com"

