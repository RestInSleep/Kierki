//
// Created by jan on 18/05/24.
//

#ifndef KIERKI_COMMON_H
#define KIERKI_COMMON_H

#include <cinttypes>
#include <set>

#define QUEUE_LENGTH 5
#define NO_OF_PLAYERS 4
#define MAX_HAND_SIZE 13
#define DECK_SIZE 52



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

enum class position_t {
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
    position_t position;
    std::set<Card> hand;
    int no_of_cards;
    int current_score;

  public:
    explicit Player(position_t position);
    void add_card(Card c);
    bool has_card(Card c);
    void remove_card(Card c);
    bool has_card_of_color(card_color_t c);
    Card get_biggest_of_color(card_color_t c);
    Card get_biggest_smaller_than(Card c);
    [[nodiscard]] position_t get_pos() const;
    [[nodiscard]] int get_no_of_cards() const;

};






uint16_t read_port(char const *string);



#endif //KIERKI_COMMON_H
