#include <sys/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

bool send_all(int sockfd, const void *buffer, size_t length) {
  size_t total_sent = 0;
  const char *buf = static_cast<const char *>(buffer);
  while (total_sent < length) {
    ssize_t sent = send(sockfd, buf + total_sent, length - total_sent, MSG_WAITALL);
    if (sent == -1) {
      if (errno == EINTR)
        continue;  // Interrupted, retry
      else if (errno == EPIPE)
        return false;  // Connection closed
      else
        return false;  // Other errors
    } else if (sent == 0) {
      return false;  // Connection closed
    }
    total_sent += sent;
  }
  return true;
}

bool recv_all(int sockfd, void *buffer, size_t length) {
  size_t total_received = 0;
  char *buf = static_cast<char *>(buffer);
  while (total_received < length) {
    ssize_t received =
        recv(sockfd, buf + total_received, length - total_received, 0);
    if (received <= 0) {
      return false;
    }
    total_received += received;
  }
  return true;
}