//
// Created by jan on 26/05/24.
//

#ifndef KIERKI_PLAYER_H
#define KIERKI_PLAYER_H

#include "common.h"
#include "cards.h"



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
    static int number_of_connected_players;
    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::mutex my_turn_mutex;
    std::condition_variable card_played_cv;
    struct sockaddr_storage client_address{};
    int timeout;
    Round* current_round;

    void reading_thread();


public:

    explicit Player(Position pos, int timeout);
    explicit Player(Position pos);

    [[nodiscard]] int get_timeout() const;
    [[nodiscard]] Position get_pos() const;
    [[nodiscard]] int get_no_of_cards() const;
    [[nodiscard]] int get_current_score() const;
    [[nodiscard]] bool is_connected() const;

    void set_socket_fd(int fd);
    void set_connected(bool c);
    void set_played_card(Card c);
    void set_timeout(int t);


    void add_card(Card c);
    void remove_card(Card c);
    [[nodiscard]] bool has_card(Card c) const;
    [[nodiscard]] bool has_card_of_color(card_color_t c) const;
    [[nodiscard]] Card get_biggest_of_color(card_color_t c) const;
    [[nodiscard]] Card get_biggest_smaller_than(Card c) const;

    void lock_connection();
    void unlock_connection();
    void lock_my_turn();
    void unlock_my_turn();

    void start_reading_thread();
    void notify_connection();
    static int no_of_connected_players();
    static void add_connected_player();
    void play_card(Trick& t);
    void set_hand(std::set<Card> h);
    void add_points(int p);
    void print_hand();
    int send_deal();
    [[nodiscard]] Round * get_current_round() const;

};



#endif //KIERKI_PLAYER_H
