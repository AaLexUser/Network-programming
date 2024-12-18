#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "dns_protocol.h"

#define DEFAULT_PORT 53
#define DEFAULT_SERVER "127.0.0.1"
/* RFC 1035: Messages carried by UDP are restricted to 512 bytes. */
#define MAX_UDP_MESSAGE_SIZE 512
/* RFC 1035: The maximum length of a domain name is 255 octets or less + \0 */
#define MAX_NAME_SIZE 256
/* RFC 1035: The maximum length of a label is 63 octets. */
#define MAX_LABEL_SIZE 63

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

unsigned char *read_name(unsigned char *reader, unsigned char *buffer,
                         int *count) {
  unsigned char *name;
  unsigned int p = 0, jumped = 0, offset;
  *count = 1;
  name = (unsigned char *)malloc(256);

  name[0] = '\0';

  // Read the names in 3www6google3com0 format
  while (*reader != 0) {
    if (*reader >= 192) {
      offset = ((*reader) << 8) + *(reader + 1) -
               49152;  // 49152 = 11000000 00000000 in binary
      reader = buffer + offset - 1;
      jumped = 1;  // we have jumped to another location so counting wont go up!
    } else {
      name[p++] = *reader;
    }

    reader = reader + 1;

    if (jumped == 0) {
      *count =
          *count +
          1;  // if we havent jumped to another location then we can count up
    }
  }

  name[p] = '\0';  // string complete
  if (jumped == 1) {
    *count =
        *count + 1;  // number of steps we actually moved forward in the packet
  }

  // Convert 3www6google3com0 to www.google.com
  unsigned char *pos = name;
  char dns_name[256];
  memset(dns_name, 0, sizeof(dns_name));
  int idx = 0;
  while (pos - name < p) {
    int len = *pos;
    pos++;
    for (int i = 0; i < len; i++) {
      dns_name[idx++] = *pos;
      pos++;
    }
    dns_name[idx++] = '.';
  }
  dns_name[idx - 1] = '\0';  // remove the last dot

  strcpy((char *)name, dns_name);
  return name;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: ./client dns_name [server] [port]" << std::endl;
    return 1;
  }
  char *dns_name = argv[1];
  char *server_ip = (argc >= 3) ? argv[2] : (char *)DEFAULT_SERVER;
  int port = (argc >= 4) ? atoi(argv[3]) : DEFAULT_PORT;

  int sockfd;
  struct sockaddr_in dest;
  unsigned char buffer[MAX_UDP_MESSAGE_SIZE];

  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) {
    perror("Socket creation failed");
    return 1;
  }

  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
      0) {
    perror("Failed to set socket timeout");
    close(sockfd);
    return 1;
  }

  memset(&dest, 0, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(port);
  if (inet_pton(AF_INET, server_ip, &dest.sin_addr) <= 0) {
    perror("Invalid server address");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  DNS_HEADER *dns = (DNS_HEADER *)buffer;
  memset(dns, 0, sizeof(DNS_HEADER));
  dns->id = (unsigned short)htons(getpid());
  dns->rd = 1;  // Recursion Desired
  dns->q_count = htons(1);

  // Point to the query portion
  unsigned char *qname = buffer + sizeof(DNS_HEADER);
  unsigned char host[MAX_NAME_SIZE];
  strcpy((char *)host, dns_name);
  to_dns_name(qname, host);

  QUESTION *qinfo = (QUESTION *)(qname + strlen((char *)qname) + 1);
  qinfo->qtype = htons(T_A);
  qinfo->qclass = htons(1);

  int packet_size =
      sizeof(DNS_HEADER) + (strlen((char *)qname) + 1) + sizeof(QUESTION);

  if (sendto(sockfd, buffer, packet_size, 0, (struct sockaddr *)&dest,
             sizeof(dest)) < 0) {
    perror("Send failed");
    close(sockfd);
    return 1;
  }

  socklen_t len = sizeof(dest);
  int n = recvfrom(sockfd, buffer, MAX_UDP_MESSAGE_SIZE, 0,
                   (struct sockaddr *)&dest, &len);
  if (n < 0) {
    perror("Receive failed or timed out");
    close(sockfd);
    return 1;
  }

  dns = (DNS_HEADER *)buffer;
  unsigned char *reader = buffer + sizeof(DNS_HEADER);

  // Skip over the question section
  for (int i = 0; i < ntohs(dns->q_count); i++) {
    reader = reader + strlen((const char *)reader) + 1 + sizeof(QUESTION);
  }

  // Read answers
  for (int i = 0; i < ntohs(dns->ans_count); i++) {
    RES_RECORD answer;
    int answer_offset = 0;
    answer.name = read_name(reader, buffer, &answer_offset);
    reader += answer_offset;

    // Read the resource record
    answer.resource = (R_DATA *)reader;
    reader += sizeof(R_DATA);
    if (ntohs(answer.resource->type) == T_A) {  // Type A record
      // Read the IP address
      unsigned char ip[4];
      memcpy(ip, reader, 4);
      reader += ntohs(answer.resource->data_len);
      // Display the result
      printf("Name: %s\n", dns_name);
      printf("Address: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    } else {
      // Skip this record
      reader += ntohs(answer.resource->data_len);
    }
    free(answer.name);
  }
  close(sockfd);
  return 0;
}
