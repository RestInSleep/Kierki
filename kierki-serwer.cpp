//
// Created by jan on 18/05/24.
//
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <algorithm>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <poll.h>
#include <thread>
#include <mutex>
#include <sstream>
#include <condition_variable>
#include "err.h"
#include "common.h"
#include "kierki-serwer.h"



Player players[4]= {Player(Position::N), Player(Position::E), Player(Position::S), Player(Position::W)};


std::mutex g_number_of_threads_mutex;
std::condition_variable g_all_players;
std::condition_variable g_can_use_thread;
int g_threads_accepting_new_connections = 0;


Options::Options() : port(0), timeout(5) {}
void Options::set_port(uint16_t p) {
    this->port = p;
}
void Options::set_filename(const std::string& f) {
        this->filename = f;

}
void Options::set_timeout(uint32_t t) {
        this->timeout = t;
}
[[nodiscard]] uint16_t Options::get_port() const {
        return this->port;
}
[[nodiscard]] std::string Options::get_filename() const {
        return this->filename;
}
[[nodiscard]] uint32_t Options::get_timeout() const {
        return this->timeout;
    }

    // Wait for all players to connect.
    // This function is called by the main thread.
    // It waits until all players have connected.
void wait_for_all_players() {
    std::unique_lock<std::mutex> lock(g_number_of_players_mutex);
    g_all_players.wait(lock, []{return Player::no_of_connected_players()  == NO_OF_PLAYERS;});
}

void send_busy(int connection_fd) {
   char busy_message[10];
   int i = 4;
   memcpy(busy_message, "BUSY", i);

   players[0].lock_connection();
   if (players[0].is_connected()) {
       players[0].unlock_connection();
       memcpy(busy_message + i, "N", 1);
       i++;
   }
    players[1].lock_connection();
    if (players[1].is_connected()) {
        players[1].unlock_connection();
         memcpy(busy_message + i, "E", 1);
         i++;
    }
    players[2].lock_connection();
    if (players[2].is_connected()) {
        players[2].unlock_connection();
         memcpy(busy_message + i, "S", 1);
         i++;
    }
    players[3].lock_connection();
    if (players[3].is_connected()) {
        players[3].unlock_connection();
         memcpy(busy_message + i, "W", 1);
         i++;
    }
    busy_message[i] = '\r';
    i++;
    busy_message[i] = '\n';
    i++;
    writen(connection_fd, busy_message, i);
}






void th_get_player(int connection_fd) {
    std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
    g_threads_accepting_new_connections++;
    lock.unlock();
    char buffer[7];
    ssize_t length_read = read(connection_fd, buffer, 6);

    if (length_read != 6) {
        //printf("Length read: %ld\n", length_read);
        close(connection_fd);
        return;
    }
    if (!check_IAM_message(buffer, length_read)) {
        //printf("Not IAM message\n");
        close(connection_fd);
        return;
    }
    char place_char = buffer[3];
    int player_no = place(place_char);

    players[player_no].lock_connection();
    if (players[player_no].is_connected()) {
        players[player_no].unlock_connection();
        send_busy(connection_fd);
        close(connection_fd);
        return;
    }
    players[player_no].set_connected(true);
    players[player_no].set_socket_fd(connection_fd);

    lock.lock();
    g_threads_accepting_new_connections--;
    lock.unlock();

    players[player_no].notify_connection();
    players[player_no].unlock_connection();

    g_all_players.notify_all();
    g_can_use_thread.notify_all();
}


void th_accept_connections(int new_connections_fd) {
    while(true) {
        std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
        g_can_use_thread.wait(lock, []{return g_threads_accepting_new_connections < MAX_ACCEPTING_THREADS;});
        lock.unlock();
        int connection_fd = accept(new_connections_fd, nullptr, nullptr);
        std::thread(th_get_player, connection_fd).detach();
    }
}









std::string get_cmd_option(char ** begin, char ** end, const std::string& option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        std::string str = *itr;
        return str;
    }
    fatal("Option %s was not given a value despite being mentioned", option.c_str());
}

bool cmd_option_exists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}


void get_options(Options& options, int argc, char * argv[]) {
    if(!cmd_option_exists(argv, argv+argc, "-f"))
    {
        fatal("No filename specified");
    }

    options.set_filename(get_cmd_option(argv, argv + argc, "-f"));

    if(cmd_option_exists(argv, argv+argc, "-p"))
    {
        options.set_port(read_port(get_cmd_option(argv, argv + argc, "-p").c_str()));
    }

    if(cmd_option_exists(argv, argv+argc, "-t"))
    {
        std::string timeout_str = get_cmd_option(argv, argv + argc, "-t");
        if (std::stoi(timeout_str) < 0) {
            fatal("Timeout must be a positive number");
        }
        options.set_timeout(std::stoi(timeout_str));
    }
}

int run_server(Options& options) {

    //TODO remove
    std::cout << "Running server with options: " << std::endl;
    std::cout << "Port: " << options.get_port() << std::endl;
    std::cout << "Filename: " << options.get_filename() << std::endl;
    std::cout << "Timeout: " << options.get_timeout() << std::endl;
    std::cout << "Server is running..." << std::endl;

    // We create a IPv6 socket and later set off IPV6_V6ONLY
    // so that the server can accept both IPv4 and IPv6 connections.
    int new_connections_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (new_connections_fd < 0) {
        syserr("cannot create a socket");
    }

    // Allow the server to accept both IPv4 and IPv6 connections.
    int off = 0;
    if (setsockopt(new_connections_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0) {
        close(new_connections_fd);
        syserr("setsockopt");
    }

    struct sockaddr_in6 server_address{};
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin6_family = AF_INET6; // IPv6 - we are going to set off IPV6_V6ONLY so that
    // the server can accept both IPv4 and IPv6 connections.
    server_address.sin6_addr = in6addr_any;// Listening on all interfaces.
    server_address.sin6_port = htons(options.get_port());


    if (bind(new_connections_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof server_address) < 0) {
        close(new_connections_fd);
        syserr("bind");
    }

    // Switch the socket to listening.
    if (listen(new_connections_fd, QUEUE_LENGTH) < 0) {
        close(new_connections_fd);
        syserr("listen");
    }

    // Find out what port the server is actually listening on.
    auto length = (socklen_t) sizeof server_address;
    if (getsockname(new_connections_fd, (struct sockaddr *) &server_address, &length) < 0) {
        syserr("getsockname");
    }
    uint16_t port = ntohs(server_address.sin6_port);
    options.set_port(port); // Update the port in case it was 0.
    return new_connections_fd;
}

void get_players(const Options& options, int new_connections_fd, const std::istream& file) {

}

std::ifstream open_file(const std::string& filename) {
    std::ifstream file;
    file.open(filename);
    if (!file.is_open()) {
        fatal("Could not open file %s", filename.c_str());
    }
    return file;
}

void game(const Options& options, int new_connections_fd) {
    std::ifstream game_file = open_file(options.get_filename());
    std::thread(th_accept_connections, new_connections_fd).detach();
    wait_for_all_players();


}


int main(int argc, char* argv[]) {
    Options options = Options();
    get_options(options, argc, argv);
    int new_connections_fd = run_server(options);
    game(options, new_connections_fd);
    return 0;
}