#include "client.h"  // Used for ChatClient class and main function
#include <arpa/inet.h>  // Used for htonl, ntohl in setupConnection, sendMessage, receiveMessages
#include <errno.h>  // Used for errno in error handling
#include <netdb.h>  // Used for getaddrinfo, freeaddrinfo in setupConnection
#include <netinet/in.h>  // Used for sockaddr_in in network functions
#include <signal.h>      // Used for signal handling with sigint_handler
#include <sys/socket.h>  // Used for socket functions like socket(), connect(), accept()
#include <unistd.h>  // Used for close(), read() etc.
#include <atomic>    // Used for atomic variables like quit_flag
#include <chrono>    // Used for sleep_for in handleUserInput
#include <cstring>   // Used for memset and other C-string functions
#include <iostream>  // Used for std::cout, std::cerr
#include <sstream>   // Used for std::ostringstream
#include <string>    // Used for std::string
#include <thread>    // Used for std::thread
#include "common.h"  // Used for send_all, recv_all helper functions

std::atomic<bool> quit_flag(false);

// function to handle SIGINT
void sigint_handler(int signum) {
  (void)signum;
  quit_flag.store(true);
}
void ChatClient::setupConnection() {
  struct addrinfo hints, *res, *p;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int status;
  if ((status = getaddrinfo(server_address.c_str(), port_str.c_str(), &hints,
                            &res)) != 0) {
    throw std::runtime_error("getaddrinfo: " +
                             std::string(gai_strerror(status)));
  }

  for (p = res; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("connect");
      continue;
    }

    break;
  }

  freeaddrinfo(res);

  if (p == NULL) {
    throw std::runtime_error("Failed to connect");
  }
}

void ChatClient::sendMessage(const std::string& message) {
  uint32_t nick_size_net = htonl(nickname.size());
  uint32_t msg_size_net = htonl(message.size());

  // Send nickname size
  if (!send_all(sockfd, &nick_size_net, sizeof(nick_size_net))) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cerr << "Failed to send nickname size.\n";
    running.store(false);
    return;
  }

  // Send nickname
  if (!send_all(sockfd, nickname.c_str(), nickname.size())) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cerr << "Failed to send nickname.\n";
    running.store(false);
    return;
  }

  // Send message size
  if (!send_all(sockfd, &msg_size_net, sizeof(msg_size_net))) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cerr << "Failed to send message size.\n";
    running.store(false);
    return;
  }

  // Send message
  if (!send_all(sockfd, message.c_str(), message.size())) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cerr << "Failed to send message.\n";
    running.store(false);
    return;
  }
}

void ChatClient::receiveMessages() {
  while (running.load() && !quit_flag.load()) {
    uint32_t nick_size_net;
    if (!recv_all(sockfd, &nick_size_net, sizeof(nick_size_net))) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Disconnected from server.\n";
      running.store(false);
      break;
    }
    uint32_t nick_size = ntohl(nick_size_net);
    if (nick_size == 0 || nick_size > 1024) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Invalid nickname size received.\n";
      running.store(false);
      break;
    }
    std::string sender_nick(nick_size, '\0');
    if (!recv_all(sockfd, &sender_nick[0], nick_size)) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Disconnected while receiving nickname.\n";
      running.store(false);
      break;
    }

    uint32_t msg_size_net;
    if (!recv_all(sockfd, &msg_size_net, sizeof(msg_size_net))) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Disconnected while receiving message size.\n";
      running.store(false);
      break;
    }
    uint32_t msg_size = ntohl(msg_size_net);
    if (msg_size == 0 || msg_size > 65536) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Invalid message size received.\n";
      running.store(false);
      break;
    }
    std::string message(msg_size, '\0');
    if (!recv_all(sockfd, &message[0], msg_size)) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Disconnected while receiving message.\n";
      running.store(false);
      break;
    }

    uint32_t date_size_net;
    if (!recv_all(sockfd, &date_size_net, sizeof(date_size_net))) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Disconnected while receiving date size.\n";
      running.store(false);
      break;
    }
    uint32_t date_size = ntohl(date_size_net);
    if (date_size == 0 || date_size > 64) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Invalid date size received.\n";
      running.store(false);
      break;
    }
    std::string date(date_size, '\0');
    if (!recv_all(sockfd, &date[0], date_size)) {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cerr << "Disconnected while receiving date.\n";
      running.store(false);
      break;
    }

    // Format the incoming message
    std::ostringstream formatted_message;
    formatted_message << "{" << date << "} [" << sender_nick << "] " << message
                      << "\n";

    if (is_typing.load()) {
      // User is typing; store the message in the queue
      std::lock_guard<std::mutex> lock(queue_mutex);
      message_queue.push(formatted_message.str());
    } else {
      // User is not typing; display the message immediately
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cout << formatted_message.str();
    }
  }
}

void ChatClient::handleUserInput() {
  std::cout << "Press 'm' to send a message or 'q' to quit.\n";

  while (running.load() && !quit_flag.load()) {
    char choice;
    std::cin >> choice;
    if (choice == 'm' || choice == 'M') {
      std::cin.ignore();      // ignore the newline after 'm'
      is_typing.store(true);  // User started typing
      std::cout << "Enter your message: ";

      std::string user_message;
      std::getline(std::cin, user_message);

      if (!user_message.empty()) {
        sendMessage(user_message);
      }

      is_typing.store(false);  // User finished typing

      // After finishing typing, display any buffered messages
      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!message_queue.empty()) {
          std::lock_guard<std::mutex> cout_lock(cout_mutex);
          std::cout << message_queue.front();
          message_queue.pop();
        }
      }
    } else if (choice == 'q' || choice == 'Q') {
      quit_flag.store(true);
      break;
    }
  }
}

void ChatClient::run() {
  try {
    setupConnection();

    std::thread recv_thread(&ChatClient::receiveMessages, this);
    handleUserInput();

    // If the socket is blocking, we need to close it to unblock any pending
    // read operations
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    if (recv_thread.joinable()) {
      recv_thread.join();
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
  }

  close(sockfd);
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " <server_address> <port> <nickname>\n";
    return 1;
  }

  // Set up the signal handler
  struct sigaction sa;
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);

  try {
    ChatClient client(argv[1], argv[2], argv[3]);
    client.run();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}