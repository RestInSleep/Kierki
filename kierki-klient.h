//
// Created by jan on 28/05/24.
//

#ifndef KIERKI_KIERKI_KLIENT_H
#define KIERKI_KIERKI_KLIENT_H

#include <string>
#include <cinttypes>
#include <set>
#include <vector>
#include "cards.h"


class Client_Options {
    std::string hostname;
    uint16_t port;
    char position;
    bool is_automatic;
    uint16_t ip_version; // 0 - unspecified, 4 - ipv4, 6 - ipv6

public:
    Client_Options();
    void set_hostname(const std::string& h);
    void set_port(uint16_t p);
    void set_position(char p);
    void set_automatic(bool a);
    void set_ip_version(uint16_t v);
    [[nodiscard]] std::string get_hostname() const;
    [[nodiscard]] uint16_t get_port() const;
    [[nodiscard]] char get_position() const;
    [[nodiscard]] bool get_automatic() const;
    [[nodiscard]] uint16_t get_ip_version() const;
};
class ClientTrick;

class ClientPlayer {
    std::set<Card> hand;
    Position position;
    std::vector<ClientTrick> taken_tricks;
    std::mutex cards_mutex;
    int current_round_type;
    bool play_now;

public:

    ClientPlayer(Position pos);
    [[nodiscard]] Position get_pos() const;
    void print_hand();
    void remove_card(Card c);
    [[nodiscard]] bool has_card(Card c) const;
    [[nodiscard]] bool has_card_of_color(card_color_t c) const;
    [[nodiscard]] Card get_biggest_of_color(card_color_t c) const;
    [[nodiscard]] Card get_biggest_smaller_than(Card c) const;
    void set_hand(const std::set<Card>& h);
    void set_current_round_type(int t);
    [[nodiscard]] int get_current_round_type() const;
    std::unique_lock<std::mutex> get_cards_lock();
    [[nodiscard]] std::set<Card> get_hand() const;
    void set_play_now(bool p);
    [[nodiscard]] bool get_play_now() const;
    void add_trick(const ClientTrick& t);
    void clean_tricks();
};

void print_card_set(const std::set<Card>& s);

void print_card_vector(const std::vector<Card>& v);

class ClientTrick {
    std::vector<Card> played_cards;

public:
    ClientTrick(const std::vector<Card>& played_cards);
    void set_played_cards(const std::vector<Card>& c);
    void print_trick() const;

};




#endif //KIERKI_KIERKI_KLIENT_H
