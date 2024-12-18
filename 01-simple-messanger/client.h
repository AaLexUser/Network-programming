#ifndef CLIENT_H
#define CLIENT_H

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <queue>
class ChatClient {
 private:
  std::string server_address;
  std::string port_str;
  std::string nickname;
  int sockfd;
  std::atomic<bool> running;
  std::mutex cout_mutex;
  std::atomic<bool> is_typing{false};
  std::queue<std::string> message_queue;  // Queue to store delayed messages
  std::mutex queue_mutex;

  void setupConnection();
  void sendMessage(const std::string &message);
  void receiveMessages();
  void handleUserInput();

 public:
  ChatClient(const std::string &address, const std::string &port,
             const std::string &nick)
      : server_address(address),
        port_str(port),
        nickname(nick),
        sockfd(-1),
        running(true) {}

  void run();
};

#endif  // CLIENT_H
