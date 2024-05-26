//
// Created by jan on 26/05/24.
//

#include "cards.h"

#include <iostream>

const std::unordered_map<card_color_t, char> color_to_char = {{card_color_t::C, 'C'}, {card_color_t::D, 'D'}, {card_color_t::H, 'H'}, {card_color_t::S, 'S'}};
const std::unordered_map<char, card_color_t> char_to_color = {{'C', card_color_t::C}, {'D', card_color_t::D}, {'H', card_color_t::H}, {'S', card_color_t::S}};
const std::unordered_map<card_value_t, std::string> value_to_string = {{card_value_t::TWO, "2"}, {card_value_t::THREE, "3"}, {card_value_t::FOUR, "4"}, {card_value_t::FIVE, "5"}, {card_value_t::SIX, "6"}, {card_value_t::SEVEN, "7"}, {card_value_t::EIGHT, "8"}, {card_value_t::NINE, "9"}, {card_value_t::TEN, "10"}, {card_value_t::J, "J"}, {card_value_t::Q, "Q"}, {card_value_t::K, "K"}, {card_value_t::A, "A"}};


Card::Card(card_color_t color, card_value_t value) : color(color), value(value) {}

Card::Card(int color, int value) : color(static_cast<card_color_t>(color)), value(static_cast<card_value_t>(value)) {}

bool Card::operator<(const Card &other) const {
    return static_cast<int>(color) < static_cast<int>(other.get_color())
           || (static_cast<int>(color) == static_cast<int>(other.get_color())
               && static_cast<int>(value) < other.num_value());
}

bool Card::operator==(const Card &other) const {
    return this->color == other.get_color() && this->value == other.get_value();
}

card_color_t Card::get_color() const {
    return this->color;
}

card_value_t Card::get_value() const {
    return this->value;
}

int Card::num_value() const {
    return static_cast<int>(this->value);
}



Trick::Trick(Position starting_player, int trick_number, int round_type) : taking_card(
        Card(card_color_t::NONE, card_value_t::NONE)) {
    this->starting_player = starting_player;
    this->current_player = starting_player;
    this->no_played_cards = 0;
    this->leading_color = 0;
    this->played_cards = std::vector<Card>();
    this->taking_player = starting_player;
    this->trick_number = trick_number;
    this->round_type = round_type;
}

Position Trick::get_starting_player() const {
    return this->starting_player;
}

Position Trick::get_current_player() const {
    return this->current_player;
}

Position Trick::get_taking_player() const {
    return this->taking_player;
}

int Trick::get_no_played_cards() const {
    return this->no_played_cards;
}

Card Trick::get_taking_card() const {
    return this->taking_card;
}

int Trick::get_leading_color() const {
    return this->leading_color;
}

void Trick::add_card(Card c) {
    if (this->no_played_cards == 0) {
        this->leading_color = static_cast<int>(c.get_color());
    }
    this->played_cards.push_back(c);
    this->no_played_cards++;
    if (c.get_color() == this->taking_card.get_color() && this->taking_card < c) {
        //TODO find out why it is treated as unreachable
        this->taking_card = c;
        this->taking_player = this->current_player;
    }
    //this->current_player = static_cast<Position>((static_cast<int>(this->current_player) + 1) % NO_OF_PLAYERS);
}

int Trick::evaluate_trick() {
    int result{0};
    switch (this->round_type) {
        case 7:
            [[fallthrough]];
        case 1:
            result += 1;
            if (this->round_type == 1) {
                break;
            }
            [[fallthrough]];
        case 2:
            for (const auto &card: this->played_cards) {
                if (card.get_color() == card_color_t::H) {
                    result++;
                }
            }
            if (this->round_type == 2) {
                break;
            }
            [[fallthrough]];
        case 3:
            for (const auto &card: this->played_cards) {
                if (card.get_value() == card_value_t::Q) {
                    result += 5;
                }
            }
            if (this->round_type == 3) {
                break;
            }
            [[fallthrough]];
        case 4:

            for (const auto &card: this->played_cards) {
                if (card.get_value() == card_value_t::K || card.get_value() == card_value_t::J) {
                    result += 2;
                }
            }
            if (this->round_type == 4) {
                break;
            }
            [[fallthrough]];
        case 5:
            for (const auto &card: this->played_cards) {
                if (card.get_value() == card_value_t::K && card.get_color() == card_color_t::H) {
                    result += 18;
                }
            }
            if (this->round_type == 5) {
                break;
            }
            [[fallthrough]];
        case 6:
            for (const auto &card: this->played_cards) {
                if (this->trick_number == 7 || this->trick_number == 13) {
                    result += 10;
                }
            }
            if (this->round_type == 6) {
                break;
            }
        default:
            break;
    }
    return result;
}

Round::Round(int round_type, Position starting_player, std::string starting_hands[4]) : scores{0, 0, 0, 0} {
    this->round_type = round_type;
    this->starting_player = starting_player;
    for (int i = 0; i < 4; i++) {
        this->starting_hands[i] = starting_hands[i];
    }
    this->finished_tricks_by_now = 0;
    this->dealt_points = 0;
}

int Round::get_round_type() const {
    return this->round_type;
}

Position Round::get_starting_player() const {
    return this->starting_player;
}

int Round::get_finished_tricks_by_now() const {
    return this->finished_tricks_by_now;
}

int Round::get_dealt_points() const {
    return this->dealt_points;
}

void Round::add_points(int p, Position pos) {
    this->scores[static_cast<int>(pos)] += p;
}

Position get_start_pos_from_sett(std::string &s) {
    size_t length = s.size();
    char start_pos = s[length - 1];
    switch (start_pos) {
        case 'N':
            return Position::N;
        case 'E':
            return Position::E;
        case 'S':
            return Position::S;
        case 'W':
            return Position::W;
        default:
            return Position::N;
    }
}

std::set<Card> create_card_set_from_string(const std::string &s) {
    std::set<Card> cards{};
    int i{0};
    while (s[i] != '\0') {
        card_value_t value;
        card_color_t color;
        switch (s[i]) {
            case '1':
                if (s[++i] != '0') {
                    value = card_value_t::NONE;
                    break;
                }
                value = card_value_t::TEN;
                break;

            case '2':
                value = card_value_t::TWO;
                break;
            case '3':
                value = card_value_t::THREE;
                break;
            case '4':
                value = card_value_t::FOUR;
                break;
            case '5':
                value = card_value_t::FIVE;
                break;
            case '6':
                value = card_value_t::SIX;
                break;
            case '7':
                value = card_value_t::SEVEN;
                break;
            case '8':
                value = card_value_t::EIGHT;
                break;
            case '9':
                value = card_value_t::NINE;
                break;
            case 'J':
                value = card_value_t::J;
                break;
            case 'Q':
                value = card_value_t::Q;
                break;
            case 'K':
                value = card_value_t::K;
                break;
            case 'A':
                value = card_value_t::A;
                break;
            default:
                value = card_value_t::NONE;
        }
        i++;
        switch (s[i]) {
            case 'C':
                color = card_color_t::C;
                break;
            case 'D':
                color = card_color_t::D;
                break;
            case 'H':
                color = card_color_t::H;
                break;
            case 'S':
                color = card_color_t::S;
                break;
            default:
                color = card_color_t::NONE;
        }
        i++;
        cards.insert(Card(color, value));
    }
    return cards;
}

void Card::print_card() const {
    std::cout << value_to_string.at(this->value) << color_to_char.at(this->color);
}

std::string Round::get_starting_hand(Position pos) const {
    return this->starting_hands[static_cast<int>(pos)];
}