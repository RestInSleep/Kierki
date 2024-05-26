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
#include <condition_variable>
#include <thread>
#include <iostream>
#include <utility>
#include "err.h"
#include "common.h"

constexpr std::string_view value_regex_string = "(10|[23456789JQKA])";
const std::string_view color_regex_string = "[CDHS]";
const std::string_view trick_number_regex_string = "[0123456789(10)(11)(12)(13)]";
const std::string_view card_regex_string = "(10|[0123456789JQKA])[CDHS]";
int Player::number_of_connected_players = 0;

std::mutex g_number_of_players_mutex;

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

bool Card::operator<(const Card &other) const {
    return static_cast<int>(color) < static_cast<int>(other.get_color())
           || (static_cast<int>(color) == static_cast<int>(other.get_color())
               && static_cast<int>(value) < other.num_value());
}

bool Card::operator==(const Card &other) const {
    return this->color == other.get_color() && this->value == other.get_value();
}

Card::Card(card_color_t color, card_value_t value) : color(color), value(value) {}

Card::Card(int color, int value) : color(static_cast<card_color_t>(color)), value(static_cast<card_value_t>(value)) {}


Player::Player(Position pos, int time) : position(pos), timeout(time), played_card(card_color_t::NONE, card_value_t::NONE) {
    auto s = std::set<Card>();
    this->hand = s;
    this->no_of_cards = 0;
    this->current_score = 0;
    this->connected = false;
    this->socket_fd = -1;
    this->my_turn = false;
    this->card_played = false;
    this->client_address = {};
}

Player::Player(Position pos): position(pos), played_card(card_color_t::NONE, card_value_t::NONE){
    auto s = std::set<Card>();
    this->hand = s;
    this->no_of_cards = 0;
    this->current_score = 0;
    this->connected = false;
    this->socket_fd = -1;
    this->my_turn = false;
    this->card_played = false;
    this->client_address = {};
    this->timeout = 5;
}

void Player::add_card(Card c) {
    if (this->no_of_cards == MAX_HAND_SIZE) {
        fatal("Too many cards in hand");
    }
    this->hand.insert(c);
    this->no_of_cards++;
}


bool Player::has_card(Card c) {
    for (const auto &card: this->hand) {
        if (card.get_color() == c.get_color() && card.get_value() == c.get_value()) {
            return true;
        }
    }
    return false;
}

bool Player::has_card_of_color(card_color_t c) {
    for (const auto &card: this->hand) {
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

    if (!this->has_card_of_color(c)) {
        return {card_color_t::NONE, card_value_t::NONE};
    }
    Card biggest(c, card_value_t::TWO);
    for (const auto &card: this->hand) {
        if (card.get_color() == c && card < biggest) {
            biggest = card;
        }
    }
    return biggest;
}

// Should be called only if has_card_of_color(c) is true
Card Player::get_biggest_smaller_than(Card c) {
    Card biggest(c.get_color(), card_value_t::TWO);
    for (const auto &card: this->hand) {

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


void Player::play_card(Trick &t) {
        // Now this player's reading thread knows it should await for a card.
    {//MIND
        std::unique_lock<std::mutex> lock_turn(this->my_turn_mutex);
        this->my_turn = true;
    }//MIND
        while (true) {

            // if we are not connected, we wait until we are - we don't want to measure time
           // if we are not connected
            {//MIND
                std::unique_lock<std::mutex> lock_conn(this->connection_mutex);
                if (!this->connected) {
                    this->connection_cv.wait(lock_conn, [this] { return this->connected; });
                }
            }//MIND

            // now, since my_turn is set, reading thread may have already received
           // a proper card and set card_played to true
           std::unique_lock<std::mutex> lock_turn(this->my_turn_mutex);

            if (!this->card_played) {
               // if we didn't yet, we wait for a card to be played until timeout
               // reading thread should be running now
               this->card_played_cv.wait_for(lock_turn, std::chrono::seconds(this->timeout),
                                             [this] { return this->card_played;});
            }

            if (this->card_played) {
                break;
            }
            else {
                // if after timeout we still didn't get a card, we disconnect
                {
                    std::lock_guard<std::mutex> lock_conn(this->connection_mutex);
                    this->connected = false;
                }
            }
        }
        t.add_card(this->played_card);
        this->remove_card(this->played_card);
        this->card_played = false;
        this->my_turn = false; //TODO - it probably should be set to false in reading thread
    }


int Player::get_no_of_cards() const {
    return this->no_of_cards;
}

void Player::set_played_card(Card c) {
    this->played_card = c;
}

void Player::set_hand(std::set<Card> h) {
    this->hand = std::move(h);
}

bool Player::is_connected() const {
    return this->connected;
}

void Player::set_connected(bool c) {
    this->connected = c;
}

int Player::get_current_score() const {
    return this->current_score;
}


void Player::start_reading_thread() {
    std::thread(&Player::reading_thread, this).detach();
}

void Player::add_points(int p) {
    this->current_score += p;
}

int Player::get_timeout() {
    return this->timeout;
}


// This thread should be started after every player is connected first.
// This is because the condition check !this->connected works well
// only after disconnection, not before connection.
void Player::reading_thread() {
    char buffer[128];
    ssize_t length_read;
    while (true) {
        std::unique_lock<std::mutex> lock(this->connection_mutex);
        if (!this->connected) {
            // hang until we are connected
            this->connection_cv.wait(lock, [this] { return this->connected; });
            // now we are connected again
            lock.unlock();
            //TODO send DEAL and TAKEN messages

        }
        std::unique_lock<std::mutex> lock_turn(this->my_turn_mutex);
        if (!this->my_turn) {
            lock_turn.unlock();
        } else { // my turn
            lock_turn.unlock();

            // if card is played
            // unlock()
            // this->card_played = true;
            // this->card_played_cv.notify_all();

        }
    }
}

void Player::lock_connection() {
    this->connection_mutex.lock();
}

void Player::unlock_connection() {
    this->connection_mutex.unlock();
}

void Player::lock_my_turn() {
    this->my_turn_mutex.lock();
}

void Player::unlock_my_turn() {
    this->my_turn_mutex.unlock();
}

void Player::set_socket_fd(int fd) {
    this->socket_fd = fd;
}

void Player::notify_connection() {
    this->connection_cv.notify_all();
}

int Player::no_of_connected_players() {
    return Player::number_of_connected_players;
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


bool check_IAM_message(const char *buffer, ssize_t length_read) {
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

int check_TRICK_from_client(const char *buffer, ssize_t length_read) {

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
    switch (length_read) {
        case 6:
            if (std::regex_match(mess, std::regex(tr_regex6))) {
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
            if (std::regex_match(mess, std::regex(tr_regex10_2)) ||
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

std::set<Card> create_card_set_from_string(const std::string &s) {
    std::set<Card> cards{};
    int i{0};
    while (s[i] != '\n') {
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

Position Round::get_starting_player() {
    return this->starting_player;
}

int Round::get_finished_tricks_by_now() const {
    return this->finished_tricks_by_now;
}

int Round::get_dealt_points() const {
    return this->dealt_points;
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

Position Trick::get_starting_player() {
    return this->starting_player;
}

Position Trick::get_current_player() {
    return this->current_player;
}

Position Trick::get_taking_player() {
    return this->taking_player;
}

int Trick::get_no_played_cards() {
    return this->no_played_cards;
}

Card Trick::get_taking_card() {
    return this->taking_card;
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

int Trick::get_leading_color() {
    return this->leading_color;
}

int get_round_type_from_sett(std::string &s) {
    size_t length = s.size();
    std::string round_type = s.substr(0, length - 2);
    return std::stoi(round_type);
}


Position get_start_pos_from_sett(std::string &s) {
    size_t length = s.size();
    char start_pos = s[length - 2];
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

void Round::add_points(int p, Position pos) {
    this->scores[static_cast<int>(pos)] += p;
}