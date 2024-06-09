//
// Created by jan on 18/05/24.
//
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <thread>
#include <mutex>
#include <boost/program_options.hpp>
#include <csignal>
#include <arpa/inet.h>


#include <condition_variable>
#include "err.h"
#include "common.h"
#include "kierki-serwer.h"
#include "player.h"
#include "cards.h"


std::mutex g_number_of_threads_mutex;
std::mutex current_round_mutex;
std::mutex current_trick_mutex;
std::condition_variable g_all_players;
std::condition_variable g_can_use_thread;

namespace po = boost::program_options;


bool all_players_connected(Player *players) {
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        std::unique_lock<std::mutex> lock = players[i].get_connection_lock();
        if (!players[i].is_connected()) {
            return false;
        }
    }
    return true;
}


Options::Options() : port(0), timeout(5) {}

void Options::set_port(uint16_t p) {
    this->port = p;
}

void Options::set_filename(const std::string &f) {
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
void wait_for_all_players(Player *players) {
    std::unique_lock<std::mutex> lock(g_number_of_players_mutex);
    if (all_players_connected(players)) {
        return;
    }
    g_all_players.wait(lock, [&players] { return all_players_connected(players); });
}

void send_busy(int connection_fd, Player *players, int pos, ReportPrinter &printer, uint16_t server_port,
               uint16_t client_port, const std::string &client_ip, const std::string &server_interface_ip) {
    char busy_message[10];
    int mess_index = static_cast<int>(strlen("BUSY"));
    memcpy(busy_message, "BUSY", strlen("BUSY"));

    for (int j = 0; j < NO_OF_PLAYERS; j++) {
        if (j == pos) {
            busy_message[mess_index++] = position_no_to_char.at(j);
        } else {
            std::unique_lock<std::mutex> lock = players[j].get_connection_lock();
            if (players[j].is_connected()) {
                busy_message[mess_index++] = position_no_to_char.at(j);
            }
        }
    }
    busy_message[mess_index++] = '\r';
    busy_message[mess_index++] = '\n';
    writen(connection_fd, busy_message, mess_index);
    std::string busy_message_str(busy_message, mess_index);
    printer.add_report_log_to_client(busy_message_str, server_interface_ip, server_port, client_ip, client_port);

    std::cerr << "Place is occupied" << std::endl;
}


void thread_get_player(int connection_fd, int &threads_accepting_new_connections,
                       Player *players, int timeout, ReportPrinter &printer, uint16_t server_port,
                       uint16_t client_port, const std::string &client_ip, const std::string &server_interface_ip) {
    {
        std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
        threads_accepting_new_connections++;
    }

    char buffer[MAX_MESSAGE_SIZE + 2];
    ssize_t current_read = 0;

    auto start = std::chrono::high_resolution_clock::now();

    while (true) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        if (elapsed_seconds.count() > timeout) {
            break;
        }
        if (current_read >= MAX_MESSAGE_SIZE) {
            break;
        }
        ssize_t length_read = read(connection_fd, buffer + current_read, 1);
        if (length_read == 0) {
            break;
        }
        current_read += length_read;
        if (buffer[current_read - 1] == '\n' && buffer[current_read - 2] == '\r') {
            break;
        }
    }
    if (buffer[current_read - 1] != '\n' || buffer[current_read - 2] != '\r') {
        buffer[current_read++] = '\r';
        buffer[current_read++] = '\n';
    }
    std::string received_message(buffer, current_read);
    printer.add_report_log_from_client(received_message, server_interface_ip, server_port, client_ip, client_port);

    if (!check_IAM_message(buffer, current_read)) {
        {
            std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
            threads_accepting_new_connections--;
        }
        close(connection_fd);
        return;
    }

    char place_char = buffer[3];
    int player_no = char_to_position_no.at(place_char);
    {
        std::unique_lock<std::mutex> con_lock = players[player_no].get_connection_lock();
        if (players[player_no].is_connected()) {
            con_lock.unlock();
            {
                std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
                threads_accepting_new_connections--;
            }
            send_busy(connection_fd, players, player_no, printer, server_port, client_port, client_ip,
                      server_interface_ip);
            close(connection_fd);
            return;
        }
        players[player_no].set_socket_fd(connection_fd);
        players[player_no].set_client_ip(client_ip);
        players[player_no].set_client_port(client_port);
        players[player_no].set_server_interface_ip(server_interface_ip);
        // if the game is ongoing, we should send DEAL and TAKEN
        std::unique_lock<std::mutex> round_lock(current_round_mutex);
        if (players[player_no].get_current_round() != nullptr) {
            round_lock.unlock();
            if (players[player_no].send_deal(printer) == -1) {
                {
                    std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
                    threads_accepting_new_connections--;
                }
                close(connection_fd);
                return;
            }

            for (auto &trick: players[player_no].get_current_round()->get_played_tricks()) {
                if (players[player_no].send_taken(trick, printer) == -1) {
                    {
                        std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
                        threads_accepting_new_connections--;
                    }
                    close(connection_fd);
                    return;
                }
            }
        }

        players[player_no].set_connected(true);
        {
            std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
            threads_accepting_new_connections--;
        }
    }

    players[player_no].notify_connection();
    g_all_players.notify_all();
    g_can_use_thread.notify_all();
}


void th_accept_connections(int new_connections_fd, Player *players, int timeout,
                           ReportPrinter &printer, uint16_t server_port) {
    int threads_accepting_new_connections = 0;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(g_number_of_threads_mutex);
            g_can_use_thread.wait(lock, [threads_accepting_new_connections] {
                return threads_accepting_new_connections < MAX_ACCEPTING_THREADS;
            });
        }
        struct sockaddr_in6 client_address{};
        struct sockaddr_in6 server_address{};
        socklen_t client_address_len = sizeof client_address;
        socklen_t server_address_len = sizeof server_address;
        int connection_fd = accept(new_connections_fd, (struct sockaddr *) &client_address, &client_address_len);
        if (connection_fd < 0) {
            close(connection_fd);
            continue;
        }
        if (getsockname(connection_fd, (struct sockaddr *) &server_address, &server_address_len) < 0) {
            close(connection_fd);
            continue;
        }
        if (getpeername(connection_fd, (struct sockaddr *) &server_address, &client_address_len) < 0) {
            close(connection_fd);
            continue;
        }
        char client_address_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &client_address.sin6_addr, client_address_str, INET6_ADDRSTRLEN) == NULL) {
            close(connection_fd);
            continue;
        }
        char server_address_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &server_address.sin6_addr, server_address_str, INET6_ADDRSTRLEN) == NULL) {
            close(connection_fd);
            continue;
        }

        std::string client_ip(client_address_str);
        std::string server_interface_ip(server_address_str);


        uint16_t client_port = ntohs(client_address.sin6_port);

        std::thread(thread_get_player, connection_fd, std::ref(threads_accepting_new_connections),
                    players, timeout, std::ref(printer), server_port, client_port, std::ref(client_ip),
                    std::ref(server_interface_ip)).detach();
    }
}


int get_options(Options &options, int argc, char *argv[]) {
    try {
        // Define the supported options
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help,h", "produce help message")
                ("port,p", po::value<int>()->default_value(0), "set port number")
                ("filename,f", po::value<std::string>()->required(), "set filename")
                ("timeout,t", po::value<int>()->default_value(5), "set timeout value");

        // Define a variable map to store the parsed options
        po::variables_map vm;

        // Parse command line arguments, disallow unrecognized options
        auto parsed = po::command_line_parser(argc, argv)
                .options(desc)
                .allow_unregistered()
                .run();

        // Store the parsed options in the variables map
        po::store(parsed, vm);

        // Check for unrecognized options and report errors
        std::vector<std::string> unrecognized = po::collect_unrecognized(parsed.options, po::include_positional);
        if (!unrecognized.empty()) {
            std::cerr << "Error: Unrecognized options: ";
            for (const auto &option: unrecognized) {
                std::cerr << option << " ";
            }
            std::cerr << "\n";
            std::cerr << "Use --help to display the allowed options.\n";
            return 1;
        }

        // Handle help option before notifying to avoid validating required options
        if (vm.count("help")) {
            std::cerr << desc << "\n";
            return 0;
        }

        // Notify to validate any required options
        po::notify(vm);

        // Access and handle options
        int port = vm["port"].as<int>();
        std::string filename = vm["filename"].as<std::string>();
        int timeout = vm["timeout"].as<int>();

        options.set_port(port);
        options.set_filename(filename);
        options.set_timeout(timeout);
        signal(SIGPIPE, SIG_IGN);

    } catch (const po::error &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        std::cerr << "Use --help to display the allowed options.\n";
        return 1;
    }
    return 0;
}

int run_server(Options &options) {
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
    server_address.sin6_family = AF_INET6; // IPv6 - we set off IPV6_V6ONLY so that
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


std::ifstream open_file(const std::string &filename) {
    std::ifstream file;
    file.open(filename);
    if (!file.is_open()) {
        fatal("Could not open file %s", filename.c_str());
    }
    return file;
}


void deal_cards(Player *players, std::string *starting_hands, ReportPrinter &printer) {
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        while (true) {
            wait_for_all_players(players); // it does not block if all players are already connected
            if (players[i].send_deal(printer) == 0) {
                break;
                // send_deal already set the is_connected of the player to false
            }
        }
        players[i].set_hand(create_card_set_from_string(starting_hands[i]));
    }
}

void set_current_round(Player *players, Round *round) {
    std::unique_lock<std::mutex> lock(current_round_mutex);
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        players[i].set_current_round(round);
    }
}

void set_current_trick(Player *players, Trick *trick) {
    std::unique_lock<std::mutex> lock(current_trick_mutex);
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        players[i].set_current_trick(trick);
    }
}

void wait_for_card(Player *players, Position trick_starting_player, int i) {
    while (true) {
        wait_for_all_players(players); // it does not block if all players are already connected
        int result = players[(static_cast<int>(trick_starting_player) + i) % NO_OF_PLAYERS].play_card();
        if (result == 0) {
            break;
        } // play_card already set the is_connected of the player to false if result == -1
    }
}

void send_scores(Player *players, Round &round) {
    std::string score = create_score(round);
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        while (true) {
            wait_for_all_players(players);
            if (players[i].send_score(score) == 0) {
                break;
            }
        }
    }
}

void send_total_scores(Player *players, int *total_scores, const Round &round) {
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        total_scores[i] += round.get_score(i);
    }
    std::string total_score = create_total_score(total_scores);
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        while (true) {
            wait_for_all_players(players);
            if (players[i].send_total_score(total_score) == 0) {
                break;
            }
        }
    }
}


void send_trick_results(Player *players, Trick &trick) {
    for (int i = 0; i < NO_OF_PLAYERS; i++) {
        while (true) {
            wait_for_all_players(players);
            if (players[i].send_taken(trick) == 0) {
                break;
            }
        }
    }
}

void settle_trick(Trick &trick, Player *players,
                  Position &trick_starting_player,
                  Round &round, int &current_round_value) {
    int trick_value = trick.evaluate_trick();
    Position trick_winner = trick.get_taking_player();
    players[static_cast<int>(trick_winner)].add_points(trick_value);
    round.add_points(trick_value, trick_winner);
    trick_starting_player = trick_winner;
    current_round_value += trick_value;
}

void run_tricks(Position trick_starting_player,
                Player *players, Round &round, int &current_round_value) {
    for (int j = 1; j <= MAX_HAND_SIZE; j++) { // number of tricks
        Trick trick = Trick(trick_starting_player, j, round.get_round_type());
        set_current_trick(players, &trick);

        for (int i = 0; i < NO_OF_PLAYERS; i++) {
            wait_for_card(players, trick_starting_player, i);
        }
        // everybody played a card
        settle_trick(trick, players, trick_starting_player, round, current_round_value);

        send_trick_results(players, trick);
        if (current_round_value == max_points_per_round.at(round.get_round_type())) {
            break;
        }
        std::unique_lock<std::mutex> round_lock(current_round_mutex);
        round.add_trick(trick);
    }
}


void game_loop(Player *players, std::ifstream &game_file, int &round_count, int *total_scores, ReportPrinter &printer) {
    std::string starting_settings;
    std::string starting_hands[4];
    while (std::getline(game_file, starting_settings)) { // there is another round!
        int current_round_value = 0;

        for (int i = 0; i < NO_OF_PLAYERS; i++) {
            std::getline(game_file, starting_hands[i]);
        }

        Round round = Round(get_round_type_from_sett(starting_settings),
                            get_start_pos_from_sett(starting_settings),
                            starting_hands);

        set_current_round(players, &round);
        deal_cards(players, starting_hands, printer);
        round_count++;

        Position trick_starting_player = round.get_starting_player();

        run_tricks(trick_starting_player, players, round, current_round_value);

        send_scores(players, round);
        send_total_scores(players, total_scores, round);
    }
}


void game(const Options &options, int new_connections_fd) {
    Player players[4] = {Player(Position::N, options.get_timeout(), options.get_port()),
                         Player(Position::E, options.get_timeout(), options.get_port()),
                         Player(Position::S, options.get_timeout(), options.get_port()),
                         Player(Position::W, options.get_timeout(), options.get_port())};
    ReportPrinter printer{};
    std::ifstream game_file = open_file(options.get_filename());
    std::thread(th_accept_connections, new_connections_fd, players, options.get_timeout(), std::ref(printer),
                options.get_port()).detach();
    wait_for_all_players(players); // if not all players are connected, blocks until they are
    std::string starting_settings;
    int round_count = 0;
    int total_scores[4] = {0, 0, 0, 0};
    game_loop(players, game_file, round_count, total_scores, printer);
}


int main(int argc, char *argv[]) {
    Options options = Options();
    if (get_options(options, argc, argv) == 1) {
        return 1;
    }
    int new_connections_fd = run_server(options);
    game(options, new_connections_fd);
    return 0;
}