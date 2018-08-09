#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <pthread.h>

#ifdef _DEBUG
#define DEBUG 1
#endif

typedef int (*p2f_cb)(struct pcap_pkthdr *, u_char *);

struct pcap_to_file {
    int fd;
    const char *bpf_str;
    const char *dest_file;
    p2f_cb cb;

    size_t blink_count;
    pthread_mutex_t blinker_mtx;
};

#define PF(bpf, file, pcap_cb) { -1, bpf, file, pcap_cb }
#define ARRAY_SIZE(arr) (size_t)(sizeof(arr)/sizeof(arr[0]))

static int p2f_icmp(struct pcap_pkthdr *packethdr, u_char *packetptr);
static int p2f_udp(struct pcap_pkthdr *packethdr, u_char *packetptr);
static int p2f_arp(struct pcap_pkthdr *packethdr, u_char *packetptr);
static int p2f_nonip(struct pcap_pkthdr *packethdr, u_char *packetptr);
static int p2f_callbacks(struct pcap_pkthdr *packethdr, u_char *packetptr);
static pcap_t *open_pcap_socket(char *device, const char *bpfstr);
static void capture_loop(pcap_t* pd, pcap_handler func);
static void parse_packet(u_char *user, struct pcap_pkthdr *packethdr, u_char *packetptr);
static void bailout(int signo);
static void build_bpf_str(char *buf, size_t bufsiz);
static void *blinker_thread(void *arg);
static void p2f_init(void);

static struct pcap_to_file p2f[] = {
    PF("icmp", "/tmp/netled.icmp", p2f_icmp),
    PF("udp", "/tmp/netled.udp", p2f_udp),
    PF("arp", "/tmp/netled.arp", p2f_arp),
    PF("(!ip)", "/tmp/netled.susp", p2f_nonip)
};

static pcap_t *pd = NULL;
static const size_t linkhdrlen = sizeof(struct ether_header);
static const size_t p2f_siz = ARRAY_SIZE(p2f);
static pthread_t blinker_thrd;


static int p2f_icmp(struct pcap_pkthdr *packethdr, u_char *packetptr)
{
    struct ip* iphdr;
#if 0
    char iphdrInfo[256]
#endif
    char srcip[32], dstip[32];

    packetptr += linkhdrlen;
    iphdr = (struct ip*)packetptr;
    strcpy(srcip, inet_ntoa(iphdr->ip_src));
    strcpy(dstip, inet_ntoa(iphdr->ip_dst));
#if 0
    sprintf(iphdrInfo, "ID:%d TOS:0x%x, TTL:%d IpLen:%d DgLen:%d",
        ntohs(iphdr->ip_id), iphdr->ip_tos, iphdr->ip_ttl,
        4*iphdr->ip_hl, ntohs(iphdr->ip_len));
#endif
    packetptr += 4*iphdr->ip_hl;

    switch (iphdr->ip_p)
    {
#if 0
        case IPPROTO_TCP:
        case IPPROTO_UDP:
#endif
            break;
        case IPPROTO_ICMP:
#ifdef DEBUG
            printf("ICMP %s -> %s\n", srcip, dstip);
#endif
            return 1;
    }

    return 0;
}

static int p2f_udp(struct pcap_pkthdr *packethdr, u_char *packetptr)
{
    struct ip* iphdr;
    char srcip[32], dstip[32];

    packetptr += linkhdrlen;
    iphdr = (struct ip *) packetptr;
    strcpy(srcip, inet_ntoa(iphdr->ip_src));
    strcpy(dstip, inet_ntoa(iphdr->ip_dst));
    packetptr += 4*iphdr->ip_hl;

    switch (iphdr->ip_p)
    {
        case IPPROTO_UDP:
#ifdef DEBUG
            printf("UDP %s -> %s\n", srcip, dstip);
#endif
            return 1;
    }

    return 0;
}

static int p2f_arp(struct pcap_pkthdr *packethdr, u_char *packetptr)
{
    struct ether_header *eth;

    if (packethdr->caplen >= sizeof(struct ether_header))
    {
        eth = (struct ether_header *) packetptr;
        switch (ntohs(eth->ether_type))
        {
            case ETHERTYPE_ARP:
#ifdef DEBUG
                printf("ARP!\n");
#endif
                return 1;
#if 0
            case ETHERTYPE_IP:
            case ETHERTYPE_IPV6:
            default:
                break;
#endif
        }
    }

    return 0;
}

static int p2f_nonip(struct pcap_pkthdr *packethdr, u_char *packetptr)
{
    struct ether_header *eth;

    if (packethdr->caplen >= sizeof(struct ether_header))
    {
        eth = (struct ether_header *) packetptr;
        switch (ntohs(eth->ether_type))
        {
            case ETHERTYPE_ARP:
            case ETHERTYPE_IP:
            case ETHERTYPE_IPV6:
                break;
            default:
                printf("SUSPICIOUS PACKET!\n");
                return 1;
        }
    }

    return 0;
}

static int p2f_callbacks(struct pcap_pkthdr *packethdr, u_char *packetptr)
{
    size_t i;

    for (i = 0; i < p2f_siz; ++i)
    {
        if (p2f[i].cb(packethdr, packetptr)) {
            pthread_mutex_lock(&p2f[i].blinker_mtx);
            if (p2f[i].blink_count < 4)
                p2f[i].blink_count += 2;
            pthread_mutex_unlock(&p2f[i].blinker_mtx);
            return 1;
        }
    }

    return 0;
}

static pcap_t *open_pcap_socket(char *device, const char *bpfstr)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* pd;
    uint32_t  srcip, netmask;
    struct bpf_program  bpf;

    if (!*device && !(device = pcap_lookupdev(errbuf)))
    {
        printf("pcap_lookupdev(): %s\n", errbuf);
        return NULL;
    }

    if ((pd = pcap_open_live(device, BUFSIZ, 1, 0, errbuf)) == NULL)
    {
        printf("pcap_open_live(): %s\n", errbuf);
        return NULL;
    }

    if (pcap_lookupnet(device, &srcip, &netmask, errbuf) < 0)
    {
        printf("pcap_lookupnet: %s\n", errbuf);
        return NULL;
    }

    if (pcap_compile(pd, &bpf, (char*)bpfstr, 0, netmask))
    {
        printf("pcap_compile(): %s\n", pcap_geterr(pd));
        return NULL;
    }

    if (pcap_setfilter(pd, &bpf) < 0)
    {
        printf("pcap_setfilter(): %s\n", pcap_geterr(pd));
        return NULL;
    }

    pcap_freecode(&bpf);

    return pd;
}

static void capture_loop(pcap_t* pd, pcap_handler func)
{
    int linktype;
 
    /* Determine the datalink layer type. */
    if ((linktype = pcap_datalink(pd)) < 0)
    {
        printf("pcap_datalink(): %s\n", pcap_geterr(pd));
        return;
    }

    switch (linktype)
    {
        case DLT_EN10MB:
            break;
        default:
            printf("Unsupported datalink (%d)\n", linktype);
            return;
    }

    /* Start capturing packets. */
    if (pcap_loop(pd, 0, func, 0) < 0)
        printf("pcap_loop failed: %s\n", pcap_geterr(pd));
}

static void parse_packet(u_char *user, struct pcap_pkthdr *packethdr, u_char *packetptr)
{
    (void) user;

    if (!p2f_callbacks(packethdr, packetptr))
        printf("%s", "Captured a packet which no handler could process ..\n");
}

static void bailout(int signo)
{
    struct pcap_stat stats;

    (void) signo;

    if (pd) {
        if (pcap_stats(pd, &stats) >= 0)
        {
            printf("%d packets received\n", stats.ps_recv);
            printf("%d packets dropped\n\n", stats.ps_drop);
        }
        pcap_close(pd);
    }

    exit(0);
}

static void build_bpf_str(char *buf, size_t bufsiz)
{
    int siz = 0;
    size_t i;
    const char link_str[] = " or ";

    for (i = 0; i < p2f_siz; ++i)
    {
        if (siz > bufsiz)
            break;
        siz += snprintf(buf + siz, bufsiz - siz, "%s%s", p2f[i].bpf_str, link_str);
    }
    if (siz > ARRAY_SIZE(link_str))
        buf[siz-4] = 0;
    if (bufsiz == 0)
        buf[siz-1] = 0;
    else
        buf[siz] = 0;
}

static void *blinker_thread(void *arg)
{
    size_t i;

    while (1) {
        usleep(250000);

        for (i = 0; i < p2f_siz; ++i) {
            pthread_mutex_lock(&p2f[i].blinker_mtx);
            if (p2f[i].blink_count) {
                if (p2f[i].blink_count && p2f[i].blink_count % 2 == 0) {
#ifdef DEBUG
                    printf("LED HIGH! %s -> %s\n", p2f[i].bpf_str, p2f[i].dest_file);
#endif
                } else {
#ifdef DEBUG
                    printf("LED LOW ! %s -> %s\n", p2f[i].bpf_str, p2f[i].dest_file);
#endif
                }
                p2f[i].blink_count--;
            }
            pthread_mutex_unlock(&p2f[i].blinker_mtx);
        }
    }

    return NULL;
}

static void p2f_init(void)
{
    size_t i;
    pthread_mutexattr_t pattr = {0};

    pthread_mutexattr_init(&pattr);
    for (i = 0; i < p2f_siz; ++i)
    {
        pthread_mutex_init(&p2f[i].blinker_mtx, &pattr);
    }
}

int main(int argc, char **argv)
{
    pthread_attr_t pattr = {0};
    char interface[32] = "";
    char bpf_str[256] = "";
    int c;

    /* Get the command line options, if any */
    while ((c = getopt (argc, argv, "hi:n:")) != -1)
    {
        switch (c)
        {
            case 'h':
                printf("usage: %s [-h] [-i INTERFACE]\n", argv[0]);
                exit(0);
                break;
            case 'i':
                strcpy(interface, optarg);
                break;
        }
    }

    p2f_init();
    pthread_attr_init(&pattr);
    pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);
    pthread_create(&blinker_thrd, &pattr, &blinker_thread, NULL);

    build_bpf_str(bpf_str, ARRAY_SIZE(bpf_str));
    printf("Berkeley Filter String: '%s'\n", bpf_str);

    /*
     * Open libpcap, set the program termination signals then start
     * processing packets.
     */
    if ((pd = open_pcap_socket(interface, bpf_str)))
    {
        signal(SIGINT, bailout);
        signal(SIGTERM, bailout);
        signal(SIGQUIT, bailout);
        capture_loop(pd, (pcap_handler)parse_packet);
        bailout(0);
    }

    kill(getpid(), SIGTERM);
}
