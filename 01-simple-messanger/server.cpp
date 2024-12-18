#include "server.h"      // Includes the Server class declaration
#include <arpa/inet.h>   // For inet_ntop and inet_pton functions
#include <errno.h>       // For error handling
#include <netdb.h>       // For network database operations (getaddrinfo)
#include <netinet/in.h>  // For Internet address family
#include <unistd.h>      // For POSIX operating system API (close)
#include <csignal>       // For signal handling
#include <cstring>       // For string manipulation functions (memset, strftime)
#include <ctime>         // For time-related functions
#include <iostream>      // For input/output operations
#include <mutex>         // For thread synchronization
#include <string>        // For std::string
#include <thread>        // For std::thread
#include "common.h"      // Includes common utilities like send_all function

std::atomic<bool> server_running(true);

// Signal handler function
void signal_handler(int signum) {
  if (signum == SIGINT) {
    std::cout << "\nReceived Ctrl+C. Shutting down server...\n";
    server_running.store(false);
  }
}

// Function to broadcast a message to all connected clients
void ClientManager::broadcastMessage(const std::string& nickname,
                                     const std::string& message,
                                     const std::string& date) {
  std::lock_guard<std::mutex> lock(clients_mutex);

  std::vector<char> buffer;
  uint32_t nickname_size = htonl(nickname.size());
  uint32_t message_size = htonl(message.size());
  uint32_t date_size = htonl(date.size());

  buffer.reserve(sizeof(nickname_size) + nickname.size() +
                 sizeof(message_size) + message.size() + sizeof(date_size) +
                 date.size());

  buffer.insert(
      buffer.end(), reinterpret_cast<char*>(&nickname_size),
      reinterpret_cast<char*>(&nickname_size) + sizeof(nickname_size));
  buffer.insert(buffer.end(), nickname.begin(), nickname.end());
  buffer.insert(buffer.end(), reinterpret_cast<char*>(&message_size),
                reinterpret_cast<char*>(&message_size) + sizeof(message_size));
  buffer.insert(buffer.end(), message.begin(), message.end());
  buffer.insert(buffer.end(), reinterpret_cast<char*>(&date_size),
                reinterpret_cast<char*>(&date_size) + sizeof(date_size));
  buffer.insert(buffer.end(), date.begin(), date.end());

  for (auto it = clients.begin(); it != clients.end();) {
    int client_sock = *it;

    // Send the entire buffer at once
    if (!send_all(client_sock, buffer.data(), buffer.size())) {
      it = clients.erase(it);
      close(client_sock);
    } else {
      ++it;
    }
  }
}

// Function to handle individual client
void ClientHandler::handle() {
  std::atomic<bool> is_alive{true};
  while (is_alive.load()) {
    // Receive nickname size
    uint32_t nickname_size_net;
    ssize_t n =
        recv_all(client_sock, &nickname_size_net, sizeof(nickname_size_net));
    if (n <= 0) {
      is_alive.store(false);
      break;
    }
    uint32_t nickname_size = ntohl(nickname_size_net);

    // Receive nickname
    std::string nickname(nickname_size, '\0');
    n = recv_all(client_sock, &nickname[0], nickname_size);
    if (n <= 0) {
      is_alive.store(false);
      break;
    }

    // Receive message size
    uint32_t message_size_net;
    n = recv_all(client_sock, &message_size_net, sizeof(message_size_net));
    if (n <= 0) {
      perror("ERROR reading message size from socket");
      break;
    }
    uint32_t message_size = ntohl(message_size_net);

    // Receive message
    std::string message(message_size, '\0');
    n = recv_all(client_sock, &message[0], message_size);
    if (n <= 0) {
      perror("ERROR reading message from socket");
      break;
    }

    // Get current time
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    char time_str[6];
    std::strftime(time_str, sizeof(time_str), "%H:%M", local_time);
    std::string date = std::string(time_str);

    // Broadcast the message to all clients
    client_manager.broadcastMessage(nickname, message, date);
  }

  // Remove client from the list and close the socket
  client_manager.removeClient(client_sock);
  close(client_sock);
}

void ClientManager::closeAllConnections() {
  std::lock_guard<std::mutex> lock(clients_mutex);
  for (int client_sock : clients) {
    close(client_sock);
  }
  clients.clear();
}

void Server::setupSocket() {
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%u", port);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;  // Support both IPv4 and IPv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;  // Use for bind

  if ((rv = getaddrinfo(nullptr, port_str, &hints, &servinfo)) != 0) {
    throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(rv)));
  }

  // Loop through all the results and bind to the first we can
  for (p = servinfo; p != nullptr; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) {
      perror("ERROR opening socket");
      continue;
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      close(sockfd);
      freeaddrinfo(servinfo);
      throw std::runtime_error("Failed to set socket options");
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      perror("ERROR on binding");
      close(sockfd);
      continue;
    }

    break;  // Successfully bound
  }

  freeaddrinfo(servinfo);

  if (p == nullptr) {
    throw std::runtime_error("Failed to bind");
  }

  if (listen(sockfd, 10) == -1) {
    perror("ERROR on listen");
    close(sockfd);
    throw std::runtime_error("Failed to listen");
  }
}

void Server::acceptConnections() {
  while (server_running.load()) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    // Use select to implement a timeout on accept
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 1;  // 1 second timeout
    timeout.tv_usec = 0;

    int ready = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout);

    if (ready == -1) {
      if (errno == EBADF) {
        // The listening socket has been closed; exit the loop gracefully
        break;
      }
      if (errno != EINTR) {
        perror("ERROR on select");
      }
      continue;  // Continue the loop on other errors
    } else if (ready == 0) {
      // Timeout, loop back to check server_running
      continue;
    }

    if (!server_running.load()) {
      break;  // Exit the loop if server is no longer running
    }

    int new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &addr_size);
    if (new_fd == -1) {
      if (errno != EINTR) {
        perror("ERROR on accept");
      }
      continue;  // Continue the loop on accept error
    }

    client_manager.addClient(new_fd);
    auto handler = std::make_unique<ClientHandler>(new_fd, client_manager);
    std::thread t([h = std::move(handler)]() mutable { h->handle(); });
    t.detach();
  }
}

void Server::run() {
  setupSocket();
  std::cout << "Server is listening on port " << port << "\n";
  std::cout << "Press Ctrl+C to stop the server\n";

  // Set up the signal handler
  std::signal(SIGINT, signal_handler);

  // Start the accept loop in a separate thread
  std::thread accept_thread(&Server::acceptConnections, this);

  // Wait for the server to be stopped
  while (server_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Wait for the accept thread to finish before closing sockfd
  if (accept_thread.joinable()) {
    accept_thread.join();
  }

  // Clean up
  std::cout << "Closing all connections...\n";
  client_manager.closeAllConnections();
  close(sockfd);

  std::cout << "Server shut down complete.\n";
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: server <port>\n";
    return 1;
  }

  uint16_t portno = std::stoi(argv[1]);

  try {
    Server server(portno);
    server.run();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
