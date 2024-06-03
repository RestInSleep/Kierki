//
// Created by jan on 26/05/24.
//

#ifndef KIERKI_PLAYER_H
#define KIERKI_PLAYER_H

#include "common.h"
#include "cards.h"


#define MAX_MESSAGE_SIZE 128

extern std::mutex g_number_of_players_mutex;

class Player {
    Position position;
    std::set<Card> hand;
    int no_of_cards;
    int current_score;
    bool connected;
    bool my_turn;
    bool card_played;
    Card played_card;
    int socket_fd;
    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::mutex my_turn_mutex;
    std::mutex write_mutex;
    std::condition_variable card_played_cv;
    struct sockaddr_storage client_address{};
    uint32_t timeout;
    Round* current_round;
    Trick* current_trick;
    void reading_thread();


public:

    explicit Player(Position pos, uint32_t time);
    explicit Player(Position pos);

    [[nodiscard]] int get_timeout() const;
    [[nodiscard]] Position get_pos() const;
    [[nodiscard]] int get_no_of_cards() const;
    [[nodiscard]] int get_current_score() const;
    [[nodiscard]] bool is_connected() const;

    void set_socket_fd(int fd);
    void set_connected(bool c);
    void set_played_card(Card c);


    void add_card(Card c);
    void remove_card(Card c);
    [[nodiscard]] bool has_card(Card c) const;
    [[nodiscard]] bool has_card_of_color(card_color_t c) const;
    [[nodiscard]] Card get_biggest_of_color(card_color_t c) const;
    [[nodiscard]] Card get_biggest_smaller_than(Card c) const;

    std::unique_lock<std::mutex> get_connection_lock();
    std::unique_lock<std::mutex> get_my_turn_lock();
    std::unique_lock<std::mutex> get_write_lock();

    void start_reading_thread();
    void notify_connection();
    int play_card();
    void set_hand(const std::set<Card>& h);
    void add_points(int p);
    void print_hand();
    int send_deal();
    int send_taken(Trick& t);
    [[nodiscard]] Round * get_current_round() const;
    void set_current_round(Round *currentRound);
    [[nodiscard]] int send_trick();
    void set_current_trick(Trick *t);
    int send_score(const std::string& s);
    int send_total_score(const std::string& s);

};

std::string create_score(Round& r);
std::string create_total_score(int* total);


#endif //KIERKI_PLAYER_H
