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

  int ttl = 2;
  int sequence = 0;

  for (; ttl < MAX_HOPS; ttl++) {
    // Socket Setup
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    int readsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

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


    char recvBuf[64] = {};

    struct sockaddr_in response_addr = {
      AF_INET,
      0,
      0
    };

    bool ready = false;
    bool timedout = false;
    int count = 0;
    int selectReturned = 0;
    fd_set readFDSet;
    struct timeval timeout;

    while (!ready) {
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      FD_ZERO(&readFDSet);
      FD_SET(readsock, &readFDSet);

      DEBUG << "Wait up to 5 Seconds" << ENDL;
      if ((selectReturned = select(readsock + 1, &readFDSet, NULL, NULL, &timeout)) < 0) {
        FATAL << "Select Failed" << ENDL;
        close(readsock);
        close(sock);
        exit(1);
      }

      DEBUG << "Select Returned: " << selectReturned << " round: " << count << "/15" << ENDL;

      if (FD_ISSET(readsock, &readFDSet)) {
        DEBUG << "Bit #" << readsock << " is set" << ENDL;
        ready = true;
      }

      if (count >= 2) {
        DEBUG << "Timeout" << ENDL;
        timedout = true;
        break;
      }

      count++;
    }

    char response_ip[32] = {};
    if (!timedout) {
      socklen_t response_addr_len = sizeof(response_addr);

      int r = recvfrom(readsock, recvBuf, 64, 0, (struct sockaddr*)&response_addr, &response_addr_len);

      if (r == -1) {
        ERROR << "Read failed" << ENDL;
      }


      inet_ntop(AF_INET, &response_addr.sin_addr, response_ip, INET_ADDRSTRLEN);
      DEBUG << "Read from Socket: " << response_ip << ENDL;
    }

    std::cout << ttl << " - " << (timedout ? "Timeout" : response_ip) << std::endl;

    close(readsock);
    close(sock);
  }
}
