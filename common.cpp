//
// Created by jan on 18/05/24.
//
#include <cinttypes>
#include <cerrno>
#include <cstdlib>
#include <algorithm>
#include <unistd.h>
#include <string>
#include <regex>
#include <sys/socket.h>
#include <string_view>
#include <iostream>
#include "err.h"
#include "common.h"

constexpr std::string_view value_regex_string = "(10|[23456789JQKA])";
const std::string_view color_regex_string = "[CDHS]";
const std::string_view trick_number_regex_string = "[0123456789(10)(11)(12)(13)]";
const std::string_view card_regex_string = "(10|[0123456789JQKA])[CDHS]";


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


 Player::Player(Position p) : position(p) {
    auto s = std::set<Card>();
    this->hand = s;
    this->no_of_cards = 0;
    this->current_score = 0;
    this->connected = false;
    this->socket_fd = -1;
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

Position Player::get_pos() const {
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


ssize_t readn(int fd, void *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;

    ptr = static_cast<char *>(vptr);
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0)
            return nread;     // When error, return < 0.
        else if (nread == 0)
            break;            // EOF

        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;         // return >= 0
}


void set_timeout(int socket_fd, int timeout) {
    struct timeval tv{};
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        syserr("setsockopt failed");
    }
}

void unset_timeout(int socket_fd) {
    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        syserr("setsockopt failed");
    }
}


bool check_IAM_message(const char* buffer, ssize_t length_read) {
    if (length_read != 6) {
        return false;
    }
    std::regex IAM_regex("IAM[NESW]\r\n");
    std::string mess{};
    mess.assign(buffer, length_read);
    return std::regex_match(mess, IAM_regex);
}

// Returns 0 if the mess,age is not a correct prefix
// Returns 1 if the mess,age is a correct prefix
// Returns 2 if the mess,age is a correct TRICK mess,age

int check_TRICK_from_client(const char* buffer, ssize_t length_read) {

    static const std::string trick = "TRICK";
    static const std::string tr_regex6 = "TRICK[1-9]";
    static const std::string tr_regex7_1 = "TRICK[1-9][23456789JQKA]";
    static const std::string tr_regex7_2 = "TRICK[1-9]1";
    static const std::string tr_regex7_3 = "TRICK1[0-3]";
    static const std::string tr_regex8_1 = "TRICK[1-9][23456789JQKA][CDHS]";
    static const std::string tr_regex8_2 = "TRICK[1-9]10";
    static const std::string tr_regex8_3 = "TRICK1[0-3][23456789JQKA]";
    static const std::string tr_regex8_4 = "TRICK1[0-3]1";
    static const std::string tr_regex9_1 = "TRICK[1-9][23456789JQKA][CDHS]\r";
    static const std::string tr_regex9_2 = "TRICK[1-9]10[CDHS]";
    static const std::string tr_regex9_3 = "TRICK1[0-3][23456789JQKA][CDHS]";
    static const std::string tr_regex9_4 = "TRICK1[0-3]10";
    static const std::string tr_regex10_1 = "TRICK[1-9][23456789JQKA][CDHS]\r\n";
    static const std::string tr_regex10_2 = "TRICK[1-9]10[CDHS]\r";
    static const std::string tr_regex10_3 = "TRICK1[0-3][23456789JQKA][CDHS]\r";
    static const std::string tr_regex10_4 = "TRICK1[0-3]10[CDHS]";
    static const std::string tr_regex11_2 = "TRICK[1-9]10[CDHS]\r\n";
    static const std::string tr_regex11_3 = "TRICK1[0-3][23456789JQKA][CDHS]\r\n";
    static const std::string tr_regex11_4 = "TRICK1[0-3]10[CDHS]\r";
    static const std::string tr_regex12_4 = "TRICK1[0-3]10[CDHS]\r\n";
    
    std::string mess{};
    mess.assign(buffer, length_read);
   switch(length_read){
       case 6:
              if(std::regex_match(mess, std::regex(tr_regex6))){
                return 1;
              }
              return 0;
       case 7:
           if (std::regex_match(mess, std::regex(tr_regex7_1)) ||
               std::regex_match(mess, std::regex(tr_regex7_2)) ||
               std::regex_match(mess, std::regex(tr_regex7_3))) {
               return 1;
           }
           return 0;
       case 8:
              if (std::regex_match(mess, std::regex(tr_regex8_1)) ||
                std::regex_match(mess, std::regex(tr_regex8_2)) ||
                std::regex_match(mess, std::regex(tr_regex8_3)) ||
                std::regex_match(mess, std::regex(tr_regex8_4))) {
                return 1;
              }
              return 0;
       case 9:
                if (std::regex_match(mess, std::regex(tr_regex9_1)) ||
                    std::regex_match(mess, std::regex(tr_regex9_2)) ||
                    std::regex_match(mess, std::regex(tr_regex9_3)) ||
                    std::regex_match(mess, std::regex(tr_regex9_4))) {
                    return 1;
                }
                return 0;
       case 10:
              if (std::regex_match(mess, std::regex(tr_regex10_1))) {
                  return 2;
              }
              if( std::regex_match(mess, std::regex(tr_regex10_2)) ||
                    std::regex_match(mess, std::regex(tr_regex10_3)) ||
                    std::regex_match(mess, std::regex(tr_regex10_4))) {
                    return 1;
                }
                return 0;
       case 11:
                if (std::regex_match(mess, std::regex(tr_regex11_2)) ||
                        std::regex_match(mess, std::regex(tr_regex11_3))) {
                        return 2;
                }
                if (std::regex_match(mess, std::regex(tr_regex11_4))) {
                    return 1;
                }
                    return 0;
       case 12:
                if (std::regex_match(mess, std::regex(tr_regex12_4))) {
                    return 1;
                }
                return 0;
       default:
           if (trick.starts_with(mess)) {
               return 1;
           }
           return 0;
   }
}

// Write n bytes to a descriptor.
ssize_t writen(int fd, const void *vptr, size_t n) {
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = static_cast<const char *>(vptr);               // Can't do pointer arithmetic on void*.
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
            return nwritten;  // error

        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

int place(char p) {
    switch (p) {
        case 'N':
            return 0;
        case 'E':
            return 1;
        case 'S':
            return 2;
        case 'W':
            return 3;
        default:
            return -1;
    }
}