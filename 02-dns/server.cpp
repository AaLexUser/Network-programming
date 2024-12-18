#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "dns_protocol.h"

#define DEFAULT_PORT 53
/* RFC 1035: Messages carried by UDP are restricted to 512 bytes. */
#define MAX_UDP_MESSAGE_SIZE 512

int sockfd;

void handle_sigint(int sig) {
  (void)sig;  // Prevent unused parameter warning
  close(sockfd);
  exit(0);
}

void to_dns_name(unsigned char *dns, unsigned char *host) {
  size_t lock = 0;
  if (host[strlen((char *)host) - 1] != '.') {
    strcat((char *)host, ".");
  }
  size_t host_len = strlen((char *)host);
  for (size_t i = 0; i < host_len; i++) {
    if (host[i] == '.') {
      *dns++ = i - lock;
      for (; lock < i; lock++) {
        *dns++ = host[lock];
      }
      lock = i + 1;
    }
  }
  *dns++ = '\0';
}

void from_dns_name(unsigned char *dns_name, unsigned char *dotted_name) {
  int i = 0, j = 0;
  while (dns_name[i] != 0) {
    int len = dns_name[i++];
    for (int k = 0; k < len; k++) {
      dotted_name[j++] = dns_name[i++];
    }
    dotted_name[j++] = '.';
  }
  dotted_name[j] = '\0';
}

int main(int argc, char *argv[]) {
  int port = DEFAULT_PORT;
  if (argc >= 2) {
    port = atoi(argv[1]);
  }
  struct sockaddr_in server_addr, client_addr;
  unsigned char buffer[MAX_UDP_MESSAGE_SIZE];

  signal(SIGINT, handle_sigint);

  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) {
    perror("Socket creation failed");
    return 1;
  }
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Bind failed");
    close(sockfd);
    return 1;
  }

  while (true) {
    socklen_t len = sizeof(client_addr);
    int n = recvfrom(sockfd, buffer, MAX_UDP_MESSAGE_SIZE, 0,
                     (struct sockaddr *)&client_addr, &len);
    if (n < 0) {
      perror("Receive failed");
      continue;
    }

    // Parse the DNS query
    DNS_HEADER *dns_header = (DNS_HEADER *)buffer;
    unsigned char *qname = buffer + sizeof(DNS_HEADER);

    // Calculate the length of the domain name
    unsigned char domain_name[256];
    from_dns_name(qname, domain_name);
    int domain_len =
        strlen((char *)domain_name) > 0 ? strlen((char *)domain_name) : 1;
    // Prepare the DNS response
    dns_header->qr = 1;                // Response
    dns_header->aa = 1;                // Authoritative Answer
    dns_header->ra = 0;                // Recursion not available
    dns_header->ans_count = htons(1);  // One answer

    /* Move to the answer section */
    unsigned char *answer_ptr = buffer + n;

    /* Copy the query name to the answer section */
    memcpy(answer_ptr, qname, strlen((char *)qname) + 1);
    answer_ptr += strlen((char *)qname) + 1;

    /* Set the resource record fields */
    R_DATA *rdata = (R_DATA *)answer_ptr;
    rdata->type = htons(T_A);
    rdata->_class = htons(1);
    rdata->ttl = htonl(300);
    rdata->data_len = htons(4);  // IPv4 address length
    answer_ptr += sizeof(R_DATA);

    // Set the IP address based on the domain name length
    unsigned char ip_address[4] = {0, 0, 0, (unsigned char)domain_len};
    memcpy(answer_ptr, ip_address, 4);
    answer_ptr += 4;

    int response_len = answer_ptr - buffer;

    if (sendto(sockfd, buffer, response_len, 0, (struct sockaddr *)&client_addr,
               len) < 0) {
      perror("Send failed");
    }
  }
  close(sockfd);
  return 0;
}
