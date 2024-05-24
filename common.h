//
// Created by jan on 18/05/24.
//

#ifndef KIERKI_COMMON_H
#define KIERKI_COMMON_H

#include <cinttypes>
#include <set>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>

#define QUEUE_LENGTH 5
#define NO_OF_PLAYERS 4
#define MAX_HAND_SIZE 13
#define DECK_SIZE 52

std::mutex g_number_of_players_mutex;

enum class card_color_t {
    ERROR = 0,
    C,
    D,
    H,
    S,
};

enum class card_value_t {
    ERROR = 0,
    TWO = 2,
    THREE,
    FOUR,
    FIVE,
    SIX,
    SEVEN,
    EIGHT,
    NINE,
    TEN,
    J,
    Q,
    K,
    A,
};

enum class Position {
    N,
    E,
    S,
    W,
};



class Card {
    card_color_t color;
    card_value_t value;

  public:
    Card(card_color_t c, card_value_t v);
    Card(int color, int value);
    bool operator <(const Card& other) const;
    [[nodiscard]] card_color_t get_color() const;
    [[nodiscard]] card_value_t get_value() const;
    [[nodiscard]] int num_value() const;
};

class Player {
    Position position;
    std::set<Card> hand;
    int no_of_cards;
    int current_score;
    bool connected;
    bool my_turn;
    int socket_fd;
    static int number_of_connected_players;
    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::mutex my_turn_mutex;
    std::condition_variable my_turn_cv;
    struct sockaddr_in client_address{};

    void reading_thread();


  public:
    explicit Player(Position position);
    void add_card(Card c);
    bool has_card(Card c);
    void remove_card(Card c);
    bool has_card_of_color(card_color_t c);
    Card get_biggest_of_color(card_color_t c);
    Card get_biggest_smaller_than(Card c);
    [[nodiscard]] Position get_pos() const;
    [[nodiscard]] int get_no_of_cards() const;
    [[nodiscard]] int get_current_score() const;
    [[nodiscard]] bool is_connected() const;
    void lock_connection();
    void unlock_connection();
    void lock_my_turn();
    void unlock_my_turn();
    void start_reading_thread();
    void set_socket_fd(int fd);
    void set_connected(bool c);
    void notify_connection();
    static int no_of_connected_players();


};

bool check_IAM_message(const char* buffer, ssize_t length_read);
int check_TRICK_from_client(const char* buffer, ssize_t length_read);

uint16_t read_port(char const *string);

ssize_t readn(int fd, void *vptr, size_t n);

ssize_t writen(int fd, const void *vptr, size_t n);

void set_timeout(int socket_fd, uint32_t timeout);

int place(char p);

#endif //KIERKI_COMMON_H
