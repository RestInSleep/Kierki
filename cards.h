//
// Created by jan on 26/05/24.
//

#ifndef KIERKI_CARDS_H
#define KIERKI_CARDS_H


#include <unordered_map>
#include <regex>
#include "common.h"


constexpr std::string_view value_regex_string = "(10|[23456789JQKA])";
const std::string_view color_regex_string = "[CDHS]";
const std::string_view trick_number_regex_string = "[123456789(10)(11)(12)(13)]";
const std::string_view card_regex_string = "(10|[23456789JQKA])[CDHS]";

const std::string trick_regex_string = "TRICK((1[0-3])|[123456789])((10|[23456789JQKA])([CDHS])){0,3}";

const std::string taken_regex_string = "TAKEN((1[0-3])|[123456789])((10|[23456789JQKA])([CDHS])){4}([NESW])";

extern const std::unordered_map<int, int> max_points_per_round;



enum class card_color_t {
    NONE = 0,
    C,
    S,
    D,
    H,
};

enum class card_value_t {
    NONE = 0,
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

extern const std::unordered_map<card_color_t, char> color_to_char;
extern const std::unordered_map<char, card_color_t> char_to_color;
extern const std::unordered_map<card_value_t, std::string> value_to_string;
extern const std::unordered_map<std::string, card_value_t> string_to_value;




enum class Position {
    N,
    E,
    S,
    W,
};

extern const std::unordered_map<char, Position> char_to_position;
extern const std::unordered_map<Position, char> position_to_char;

class Card {
    card_color_t color;
    card_value_t value;

public:
    Card(card_color_t c, card_value_t v);
    Card(int color, int value);
    bool operator <(const Card& other) const;
    bool operator ==(const Card& other) const;
    [[nodiscard]] card_color_t get_color() const;
    [[nodiscard]] card_value_t get_value() const;
    [[nodiscard]] int num_value() const;
    void print_card() const;
};



class Trick {
    int no_played_cards;
    int trick_number;
    Card taking_card;
    Position starting_player;
    Position current_player;
    Position taking_player;
    card_color_t leading_color;
    std::vector<Card> played_cards;
    int round_type;

public:
    Trick(Position starting_player, int trick_number, int round_type);
    [[nodiscard]] Position get_starting_player() const;
    [[nodiscard]] Position get_current_player() const;
    [[nodiscard]] Position get_taking_player() const;
    [[nodiscard]] int get_no_played_cards() const;
    [[nodiscard]] Card get_taking_card() const;
    [[nodiscard]] card_color_t get_leading_color() const;
    void add_card(Card c);
    int evaluate_trick();
    std::vector<Card>& get_played_cards();
    [[nodiscard]] int get_trick_number() const;
    [[nodiscard]] int get_round_type() const;
};

class Round {
    int round_type;
    Position starting_player;
    int finished_tricks_by_now;
    int dealt_points;
    std::string starting_hands[4];
    std::vector<Trick> played_tricks;
    int scores[4];

public:
    Round(int round_type, Position starting_player, std::string starting_cards[4]);
    [[nodiscard]] Position get_starting_player() const;
    [[nodiscard]] int get_round_type() const;
    [[nodiscard]] int get_finished_tricks_by_now() const;
    [[nodiscard]] int get_dealt_points() const;
    void add_points(int p, Position pos);
    [[nodiscard]] std::string get_starting_hand(Position pos) const;
    std::vector<Trick>& get_played_tricks();
    int get_score(int i) const;
    void add_trick(const Trick& t);
};

Position get_start_pos_from_sett(std::string& s);

std::set<Card> create_card_set_from_string(const std::string& s);

std::vector<Card> create_card_vector_from_string(const std::string& s);



#endif //KIERKI_CARDS_H
