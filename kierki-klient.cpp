//
// Created by jan on 28/05/24.
//
#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <netdb.h>
#include <csignal>
#include "kierki-klient.h"
#include "err.h"
#include "common.h"
#include "player.h"

namespace po = boost::program_options;

Client_Options::Client_Options() {
    hostname = "";
    port = 0;
    position = 0;
    is_automatic = false;
    ip_version = 0;
}

void Client_Options::set_hostname(const std::string& h) {
    hostname = h;
}

void Client_Options::set_port(uint16_t p) {
    port = p;
}

void Client_Options::set_position(char p) {
    position = p;
}

void Client_Options::set_automatic(bool a) {
    is_automatic = a;
}

void Client_Options::set_ip_version(uint16_t v) {
    ip_version = v;
}

std::string Client_Options::get_hostname() const {
    return hostname;
}

uint16_t Client_Options::get_port() const {
    return port;
}

char Client_Options::get_position() const {
    return position;
}

bool Client_Options::get_automatic() const {
    return is_automatic;
}

uint16_t Client_Options::get_ip_version() const {
    return ip_version;
}


ClientPlayer::ClientPlayer(Position pos) {
    position = pos;
    current_round_type = 0;
}


void print_card_set(const std::set<Card>& s) {
    if (s.empty()) {
        return;
    }

    auto last = --s.end();
    for (auto it = s.begin(); it != s.end(); ++it) {
        it->print_card();
        if (it != last) {
            std::cout << ", ";
        }
    }
}

void ClientPlayer::print_hand() {
    print_card_set(hand);
}

void ClientPlayer::remove_card(Card c) {
    hand.erase(c);
}

bool ClientPlayer::has_card(Card c) const {
    return hand.find(c) != hand.end();
}

bool ClientPlayer::has_card_of_color(card_color_t c) const {
    for (const auto &card: hand) {
        if (card.get_color() == c) {
            return true;
        }
    }
    return false;
}

Card ClientPlayer::get_biggest_of_color(card_color_t c) const {
    if (!has_card_of_color(c)) {
        return {card_color_t::NONE, card_value_t::NONE};
    }
    Card biggest(c, card_value_t::TWO);
    for (const auto &card: hand) {
        if (card.get_color() == c && card < biggest) {
            biggest = card;
        }
    }
    return biggest;
}

bool ClientPlayer::get_play_now() const {
    return play_now;
}

void ClientPlayer::set_play_now(bool p) {
    play_now = p;
}



int get_options(Client_Options& options ,int argc, char* argv[]) {
    try {
        // Define the supported options
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("hostname,h", po::value<std::string>()->required(), "set hostname")
                ("-N", "north position")
                ("-S", "south position")
                ("-W", "west position")
                ("-E", "east position")
                ("port,p", po::value<int>()->required(), "set port number")
                ("4", "IPv4")
                ("6", "IPv6")
                ("-a", "automatic mode");

        po::variables_map vm;

        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }
        po::notify(vm);
        std::vector<std::string> args(argv + 1, argv + argc);
        char position = '\0';

        for (const auto& arg : args) {
            if (arg == "-N") { position = 'N'; break; }
            if (arg == "-S") { position = 'S'; break; }
            if (arg == "-W") { position = 'W'; break; }
            if (arg == "-E") { position = 'E'; break; }
        }


        if (position == '\0') {
            std::cerr << "Error: At least one position option (-N, -S, -W, -E) must be specified.\n";
            return 1;
        }

        uint16_t ip = 0;
        for (const auto& arg : args) {
            if (arg == "-4" ) { ip = 4; break; }
            if (arg == "-6" ) { ip = 6; break; }
        }

        std::string hostname = vm["hostname"].as<std::string>();
        int port = vm["port"].as<int>();
        bool automatic = vm.count("-a") > 0;
        std::cout << "automatic: " << automatic << "\n";

        options.set_ip_version(ip);
        options.set_hostname(hostname);
        options.set_port(port);
        options.set_position(position);
        options.set_automatic(automatic);
        signal(SIGPIPE, SIG_IGN);
    } catch (const po::error &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        std::cerr << "Use --help to display the allowed options.\n";
        return 1;
    }
    return 0;
}


int connect_to_server(const Client_Options& options) {
    struct addrinfo hints{};
    memset(&hints, 0, sizeof(struct addrinfo));
    if (options.get_ip_version() == 4) {
        hints.ai_family = AF_INET;  // IPv4
    } else if (options.get_ip_version() == 6) {
        hints.ai_family = AF_INET6;  // IPv6
    } else {
        hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
    }

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo* address_result;
    int errcode = getaddrinfo(options.get_hostname().c_str(), nullptr, &hints, &address_result);
    if (errcode != 0) {
        freeaddrinfo(address_result);
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }
    struct sockaddr_in send_address{};
    if (options.get_ip_version() == 0) {
        send_address.sin_family = address_result->ai_family; // unspecified desired ip version
    }
    else {
        send_address.sin_family = hints.ai_family; // specified ip version
    }
    send_address.sin_addr.s_addr = ((struct sockaddr_in*)(address_result->ai_addr))->sin_addr.s_addr; // IP address
    send_address.sin_port = htons(options.get_port()); // port from the command line
    freeaddrinfo(address_result);
    int sock = socket(send_address.sin_family, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Error: cannot create a socket\n";
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&send_address, sizeof(send_address)) < 0) {
        std::cerr << "Error: cannot connect to the server\n";
        return -1;
    }
    return sock;
}

void print_busy_places(const std::string& message) {
    std::cout << "Place busy, list of busy places received: ";
    int i = 4;
    while (message[i] != '\r') {
        std::cout << message[i++];
    }
    std::cout << ".\n";
}

void ClientPlayer::set_hand(const std::set<Card>& h) {
    hand = h;
}

void ClientPlayer::set_current_round_type(int t) {
    current_round_type = t;
}

std::unique_lock<std::mutex> ClientPlayer::get_cards_lock() {
    return std::unique_lock(cards_mutex);
}


void print_deal_message(ClientPlayer& player, char starting_player) {
    std::cout << "New deal << " << player.get_current_round_type() << ": staring place " << starting_player << ", cards ";
    player.print_hand();
    std::cout << ".\n";

}


// returns 1 if server can disconnect in the next read, 0 otherwise
int process_message(const std::string& message, ClientPlayer& player) {
    if (message.starts_with("BUSY")) {
        print_busy_places(message);
    }
    else if (message.starts_with("DEAL")) {
        int round_type = message[4] - '0';
        char starting_player = message[5];
        std::string hand_str = message.substr(6);
        auto length = hand_str.size();
        hand_str.erase(length - 2, 2);
        std::set<Card> hand = create_card_set_from_string(hand_str);
        {
            std::unique_lock<std::mutex> lock = player.get_cards_lock();
            player.set_hand(hand);
            player.set_current_round_type(round_type);
        }
        print_deal_message(player, starting_player);
        player.set_play_now(false);

    }
    else if (message.starts_with("WRONG")) {
        auto length = message.size();
        std::string wrong_str = message;
        wrong_str.erase(length - 2, 2);
        wrong_str = wrong_str.substr(5);
        std::cout << "Wrong message received in trick " << wrong_str << ".\n";
        player.set_play_now(false);
    }
    else if (message.starts_with("TAKEN")) {
        std::string taken_message= message.substr(5);
        std::string trick_number = taken_message[0] == '1' ? taken_message.substr(0, 2) : taken_message.substr(0, 1);
        size_t trick_number_length = trick_number.size();
        taken_message.erase(0, trick_number_length);
        size_t length = taken_message.size();
        char position = taken_message[length - 3];
        std::string cards_str = taken_message.substr(0, length - 3);
        std::set<Card> cards = create_card_set_from_string(cards_str);
        std::cout << "A trick " << trick_number << " is taken by " << position << " with cards ";
        print_card_set(cards);
        std::cout << ".\n";
        if (char_to_position.at(position) == player.get_pos()) {

        }



    }
}




int send_IAM(int sock, char position) {
    std::string message = "IAM";
    message += position;
    message += "\r\n";
    ssize_t bytes_sent = writen(sock, message.c_str(), message.size());
    if (bytes_sent < message.size()) {
        std::cerr << "Error writing to socket\n";
        return -1;
    }
    return 0;
}

int receive_messages(int sock, ClientPlayer& player) {
    std::vector<char> buffer(128);
    std::string data;

    while (true) {
        ssize_t bytes_read = read(sock, buffer.data(), buffer.size());
        if (bytes_read < 0) {
            std::cerr << "Error reading from socket" << std::endl;
            return -1;
        } else if (bytes_read == 0) {
            std::cerr << "Connection closed by peer" << std::endl;
            return -1;
        }

        data.append(buffer.data(), bytes_read);

        size_t pos;
        while ((pos = data.find("\r\n")) != std::string::npos) {
            std::string message = data.substr(0, pos);
            std::cout << "Received: " << message << std::endl;
            data.erase(0, pos + 2); // Erase the processed message and the delimiter
            process_message(message, player);
            // other thread should not alert main thread about losing connection - main will sooner or later also
            // receive the message about losing connection, and we don't want to lose information about messages
        }
    }
}





int play_automatic(const Client_Options& options) {
    Player player = Player(char_to_position.at(options.get_position()));
    int sock = connect_to_server(options);

}

int play_manual(const Client_Options& options) {
    ClientPlayer player = ClientPlayer(char_to_position.at(options.get_position()));
    int sock = connect_to_server(options);
    if (sock == -1) {
        return 1;
    }
    if (send_IAM(sock, options.get_position()) == -1) {
        return 1;
    }
    receive_messages(sock, player);

    return 0;
}




int play(const Client_Options& options) {
    if (options.get_automatic()) {
       return play_automatic(options);
    } else {
       return play_manual(options);
    }
}




int main(int argc, char* argv[]) {
    Client_Options options = Client_Options();
    if (get_options(options, argc, argv) == 1) {
        return 1;
    }
    return play(options);

}