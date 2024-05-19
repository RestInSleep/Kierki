//
// Created by jan on 18/05/24.
//
#include <cinttypes>
#include <cerrno>
#include <cstdlib>
#include <algorithm>
#include "err.h"
#include "common.h"



uint16_t read_port(char const *string) {
char *endptr;
errno = 0;
unsigned long port = strtoul(string, &endptr, 10);
if (errno != 0 || *endptr != 0 || port == 0 || port > UINT16_MAX) {
fatal("%s is not a valid port number", string);
}
return (uint16_t) port;
}

int Card::num_value() const {
    return static_cast<int>(this->value);
}

card_color_t Card::get_color() const {
    return this->color;
}

card_value_t Card::get_value() const {
    return this->value;
}

bool Card::operator <(const Card& other) const {
    return static_cast<int>(color) < static_cast<int>(other.get_color())
    || (static_cast<int>(color)  == static_cast<int>(other.get_color())
        && static_cast<int>(value) < other.num_value());
}

Card::Card(card_color_t color, card_value_t value) : color(color), value(value){}

Card::Card(int color, int value) : color(static_cast<card_color_t>(color)), value(static_cast<card_value_t>(value)){}


 Player::Player(position_t p) : position(p) {
    auto s = std::set<Card>();
    this->hand = s;
    this->no_of_cards = 0;
    this->current_score = 0;
}

void Player::add_card(Card c) {
    if (this->no_of_cards == MAX_HAND_SIZE) {
        fatal("Too many cards in hand");
    }
    this->hand.insert(c);
    this->no_of_cards++;
}



bool Player::has_card(Card c) {
    for (const auto& card : this->hand) {
        if (card.get_color() == c.get_color() && card.get_value() == c.get_value()) {
            return true;
        }
    }
    return false;
}

bool Player::has_card_of_color(card_color_t c) {
    for (const auto& card : this->hand) {
        if (card.get_color() == c) {
            return true;
        }
    }
    return false;
}

position_t Player::get_pos() const {
    return this->position;
}

// Should be called only if has_card(c) is true
void Player::remove_card(Card c) {
    if (this->no_of_cards == 0) {
        fatal("No cards in hand");
    }
    if (!this->has_card(c)) {
        fatal("Card not in hand");
    }
    this->hand.erase(c);
    this->no_of_cards--;
}

// Must be called only if has_card_of_color(c) is true
Card Player::get_biggest_of_color(card_color_t c) {

    if(!this->has_card_of_color(c)){
        return {card_color_t::ERROR, card_value_t::ERROR};
    }
    Card biggest(c, card_value_t::TWO);
    for (const auto& card : this->hand) {
        if (card.get_color() == c && card < biggest) {
            biggest = card;
        }
    }
    return biggest;
}

// Should be called only if has_card_of_color(c) is true
Card Player::get_biggest_smaller_than(Card c) {
    Card biggest(c.get_color(), card_value_t::TWO);
    for (const auto& card : this->hand) {

        // Cards are sorted on the  hand, so we know that
        // if we find a card with the same color as c and
        // smaller value than c, it is the biggest card
        // smaller than c
        if (card.get_color() == c.get_color() && card < c) {
            biggest = card;
        }
    }
    return biggest;
}

int Player::get_no_of_cards() const {
    return this->no_of_cards;
}
