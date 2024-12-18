#ifndef SERVER_H
#define SERVER_H

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class ClientManager {
 private:
  std::vector<int> clients;
  std::mutex clients_mutex;

 public:
  void addClient(int client_sock) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.push_back(client_sock);
  }

  void removeClient(int client_sock) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(std::remove(clients.begin(), clients.end(), client_sock),
                  clients.end());
  }

  void broadcastMessage(const std::string& nickname, const std::string& message,
                        const std::string& date);

  void closeAllConnections();
};

class ClientHandler {
 private:
  int client_sock;
  ClientManager& client_manager;

 public:
  ClientHandler(int sock, ClientManager& manager)
      : client_sock(sock), client_manager(manager) {}

  void handle();
};

class Server {
 private:
  uint16_t port;
  int sockfd;
  ClientManager client_manager;

  void setupSocket();
  void acceptConnections();

 public:
  Server(uint16_t port) : port(port), sockfd(-1) {}
  void run();
};

#endif  // SERVER_H