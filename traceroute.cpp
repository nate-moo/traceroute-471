#include "traceroute.h"
#define MAX_HOPS 30

// ****************************************************************************
// * Compute the Internet Checksum over an arbitrary buffer.
// * (written with the help of ChatGPT 3.5)
// ****************************************************************************
uint16_t checksum(unsigned short *buffer, int size) {
    unsigned long sum = 0;
    while (size > 1) {
        sum += *buffer++;
        size -= 2;
    }
    if (size == 1) {
        sum += *(unsigned char *) buffer;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short) (~sum);
}

struct packet {
  struct icmphdr header;
  char data[64 - sizeof(icmphdr)];
};

int main (int argc, char *argv[]) {
  std::string destIP;

  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  int opt = 0;
  while ((opt = getopt(argc,argv,"t:d:")) != -1) {

    switch (opt) {
    case 't':
      destIP = optarg;
      break;
    case 'd':
      LOG_LEVEL = atoi(optarg);;
      break;
    case ':':
    case '?':
    default:
      std::cout << "usage: " << argv[0] << " -t [target ip] -d [Debug Level]" << std::endl;
      exit(-1);
    }
  }

  DEBUG << "Target IP: " << destIP << ENDL;

  int ttl = 255;
  int sequence = 0;

  // for (int currentHop = 0; currentHop < MAX_HOPS; currentHop++) {
  //
  // }

  // Socket Setup
  int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

  if (sock == -1) {
    ERROR << "Socket failed to initialize" << ENDL;
    exit(1);
  }

  if (setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl) != 0)) {
    ERROR << "Failed to set socket TTL" << ENDL;
  }

  DEBUG << "Setup Socket" << ENDL;

  struct packet datagram{};
  struct sockaddr_in addr = {
    AF_INET,
    0,
    0
  };

  if (inet_pton (AF_INET, destIP.c_str(), &(addr.sin_addr)) < 0) {
    FATAL << "Failed to set IP (Is it formatted correctly?)" << ENDL;
    exit (EXIT_FAILURE);
  }

  memset(&datagram, 0, sizeof(datagram));

  datagram.header.type = ICMP_ECHO;
  datagram.header.un.echo.id = htons(getpid());
  datagram.header.un.echo.sequence = htons(sequence++);

  for (int i = 0; i < sizeof(datagram.data); i++) {
    datagram.data[i] = '0' + i;
  }

  // DEBUG << checksum(&datagram, sizeof(datagram)) << '\n' << checksum((unsigned short*)&datagram, sizeof(datagram)) << ENDL;

  datagram.header.checksum = checksum((unsigned short*)&datagram, sizeof(datagram));

  DEBUG << "Built packet" << ENDL;

  const auto s = sendto(sock, &datagram, sizeof(datagram), 0, (struct sockaddr*)&addr, sizeof(addr));

  if (s == -1) {
    ERROR << "Failed to send packet" << ENDL;
    close(sock);
    exit(1);
  }
  DEBUG << "Sent Packet: " << s << ENDL;

  // int readsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  //
  // char* recvBuf = (char*)malloc(256);
  //
  // while (
  //   recvfrom(readsock, recvBuf, 256, 0, (struct sockaddr*)&dest_addr, (socklen_t *)sizeof(dest_addr)) <= 0
  // ) {
  //
  // }
  //
  // DEBUG << dest_addr.sin_addr.s_addr << ENDL;
  // close(readsock);
  close(sock);


}
