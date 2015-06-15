#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
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

static pcap_t *pd;
static const size_t linkhdrlen = sizeof(struct ether_header);

pcap_t *open_pcap_socket(char *device, const char *bpfstr)
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
  // Open the device for live capture, as opposed to reading a packet
  // capture file.
  if ((pd = pcap_open_live(device, BUFSIZ, 1, 0, errbuf)) == NULL)
  {
    printf("pcap_open_live(): %s\n", errbuf);
    return NULL;
  }
  // Get network device source IP address and netmask.
  if (pcap_lookupnet(device, &srcip, &netmask, errbuf) < 0)
  {
    printf("pcap_lookupnet: %s\n", errbuf);
    return NULL;
  }
  // Convert the packet filter epxression into a packet
  // filter binary.
  if (pcap_compile(pd, &bpf, (char*)bpfstr, 0, netmask))
  {
    printf("pcap_compile(): %s\n", pcap_geterr(pd));
    return NULL;
  }
  // Assign the packet filter to the given libpcap socket.
  if (pcap_setfilter(pd, &bpf) < 0)
  {
    printf("pcap_setfilter(): %s\n", pcap_geterr(pd));
    return NULL;
  }

  return pd;
}

void capture_loop(pcap_t* pd, int packets, pcap_handler func)
{
  int linktype;
 
  // Determine the datalink layer type.
  if ((linktype = pcap_datalink(pd)) < 0)
  {
    printf("pcap_datalink(): %s\n", pcap_geterr(pd));
    return;
  }
  // Set the datalink layer header size.
  switch (linktype)
  {
    case DLT_EN10MB:
      break;
    default:
      printf("Unsupported datalink (%d)\n", linktype);
      return;
  }
  // Start capturing packets.
  if (pcap_loop(pd, packets, func, 0) < 0)
    printf("pcap_loop failed: %s\n", pcap_geterr(pd));
}

void parse_packet(u_char *user, struct pcap_pkthdr *packethdr, u_char *packetptr)
{
  struct ether_header *eth;
  struct ip* iphdr;
  char iphdrInfo[256], srcip[256], dstip[256];
 
  if (packethdr->caplen >= sizeof(struct ether_header)) {
    eth = (struct ether_header*)packetptr;
    switch (ntohs(eth->ether_type)) {
      case ETHERTYPE_ARP:
        printf("ARP!\n");
        break;
      case ETHERTYPE_IP:
        break;
      case ETHERTYPE_IPV6:
        break;
      default:
        break;
    }
  }
  packetptr += linkhdrlen;
  iphdr = (struct ip*)packetptr;
  strcpy(srcip, inet_ntoa(iphdr->ip_src));
  strcpy(dstip, inet_ntoa(iphdr->ip_dst));
  sprintf(iphdrInfo, "ID:%d TOS:0x%x, TTL:%d IpLen:%d DgLen:%d",
            ntohs(iphdr->ip_id), iphdr->ip_tos, iphdr->ip_ttl,
            4*iphdr->ip_hl, ntohs(iphdr->ip_len));
 
  // Advance to the transport layer header then parse and display
  // the fields based on the type of hearder: tcp, udp or icmp.
  packetptr += 4*iphdr->ip_hl;
  switch (iphdr->ip_p)
  {
    case IPPROTO_TCP:
      break;
    case IPPROTO_UDP:
      printf("UDP %s -> %s\n", srcip, dstip);
      break;
    case IPPROTO_ICMP:
      printf("ICMP %s -> %s\n", srcip, dstip);
      break;
  }
}

void bailout(int signo)
{
  struct pcap_stat stats;
 
  if (pcap_stats(pd, &stats) >= 0)
  {
     printf("%d packets received\n", stats.ps_recv);
     printf("%d packets dropped\n\n", stats.ps_drop);
  }
  pcap_close(pd);
  exit(0);
}

int main(int argc, char **argv)
{
  char interface[256] = "", bpfstr[256] = "";
  int packets = 0, c, i;
 
  // Get the command line options, if any
  while ((c = getopt (argc, argv, "hi:n:")) != -1)
  {
    switch (c)
    {
      case 'h':
        printf("usage: %s [-h] [-i ] [-n ] []\n", argv[0]);
        exit(0);
        break;
      case 'i':
        strcpy(interface, optarg);
        break;
      case 'n':
        packets = atoi(optarg);
        break;
    }
  }

  // Get the packet capture filter expression, if any.
  for (i = optind; i < argc; i++)
  {
    strcat(bpfstr, argv[i]);
    strcat(bpfstr, " ");
  }
  // Open libpcap, set the program termination signals then start
  // processing packets.
  if ((pd = open_pcap_socket(interface, bpfstr)))
  {
    signal(SIGINT, bailout);
    signal(SIGTERM, bailout);
    signal(SIGQUIT, bailout);
    capture_loop(pd, packets, (pcap_handler)parse_packet);
    bailout(0);
  }
  exit(0);
}

