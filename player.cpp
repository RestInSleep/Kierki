//
// Created by jan on 26/05/24.
//


#include <iostream>
#include <sstream>
#include "player.h"
#include "err.h"
#include "cards.h"


std::mutex g_number_of_players_mutex;

Player::Player(Position pos, uint32_t time) : position(pos), timeout(time), played_card(card_color_t::NONE, card_value_t::NONE) {
    auto s = std::set<Card>();
    this->hand = s;
    this->no_of_cards = 0;
    this->current_score = 0;
    this->connected = false;
    this->socket_fd = -1;
    this->my_turn = false;
    this->card_played = false;
    this->client_address = {};
    this->current_round = nullptr;
    this->current_trick = nullptr;
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
    this->current_round = nullptr;
    this->current_trick = nullptr;
}


// This thread should be started after every player is connected first.
// This is because the condition check !this->connected works well
// only after disconnection, not before connection.
//TODO get working
void Player::reading_thread() {
    char buffer[128];
    ssize_t length_read;
    while (true) {
        std::unique_lock<std::mutex> lock(this->connection_mutex);
        if (!this->connected) {
            // hang until we are connected
            this->connection_cv.wait(lock, [this] { return this->connected;});
            // now we are connected again
        }
        lock.unlock();
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


void Player::add_card(Card c) {
    if (this->no_of_cards == MAX_HAND_SIZE) {
        fatal("Too many cards in hand");
    }
    this->hand.insert(c);
    this->no_of_cards++;
}


bool Player::has_card(Card c) const {
    for (const auto &card: this->hand) {
        if (card.get_color() == c.get_color() && card.get_value() == c.get_value()) {
            return true;
        }
    }
    return false;
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

bool Player::has_card_of_color(card_color_t c) const {
    for (const auto &card: this->hand) {
        if (card.get_color() == c) {
            return true;
        }
    }
    return false;
}

// Should be called only if has_card_of_color(c) is true
Card Player::get_biggest_of_color(card_color_t c) const {
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

Card Player::get_biggest_smaller_than(Card c) const {
    Card biggest(c.get_color(), card_value_t::TWO);
    for (const auto &card: this->hand) {

        // Cards are sorted on the  hand, so we know that
        // if we find a card with the same color as c and
        // smaller value than c, it is the biggest card
        // smaller than c by now
        if (card.get_color() == c.get_color() && card < c) {
            biggest = card;
        }
    }
    return biggest;
}


int Player::send_trick() {
   std::cout << "Sending trick" << std::endl;
    std::stringstream ss;
    ss << "TRICK";
    ss << current_trick->get_trick_number();
    for (const auto &c: current_trick->get_played_cards()) {
        ss << value_to_string.at(c.get_value());
        ss << color_to_char.at(c.get_color());
    }
    std::string s = ss.str();
    ssize_t length = static_cast<ssize_t>(s.size());
    if (writen(this->socket_fd, s.c_str(), length) < length) {
        std::cout << "Connection closed" << std::endl;
        std::unique_lock<std::mutex> lock_conn(this->connection_mutex); //TODO - blocking here
        std::cout << "Connection closed" << std::endl;
        this->connected = false;
        return -1;
    }

    return 0;
}

// returns 0 if card was played, -1 if connection was closed or we need to ping
int Player::play_card() {

    std::unique_lock<std::mutex> lock_turn(this->my_turn_mutex);
    my_turn = true;

    std::unique_lock<std::mutex> lock_write(this->write_mutex);
    if (this->send_trick() < 0) {
       std::cout << "Connection closed2" << std::endl;
        return -1;
    }
    // now, since my_turn is set, reading thread may have already received
    // a proper card and set card_played to true

    if (!this->card_played) {
        // if we didn't yet, we wait for a card to be played until timeout
        // reading thread should be running now
        this->card_played_cv.wait_for(lock_turn, std::chrono::seconds(this->timeout),
                                          [this] {return this->card_played;});
        }
        if (!this->card_played) {
            return -1;
        }

        current_trick->add_card(this->played_card);
        this->remove_card(this->played_card);
        this->card_played = false;
        this->my_turn = false; //TODO - it probably should be set to false in reading thread
        return 0;
}

int Player::get_no_of_cards() const {
    return this->no_of_cards;
}

void Player::set_played_card(Card c) {
    this->played_card = c;
}

void Player::set_hand(const std::set<Card>& h) {
    this->hand = h;
}

bool Player::is_connected() const {
    return this->connected;
}

void Player::set_connected(bool c) {
    this->connected = c;
}

void Player::start_reading_thread() {
    std::thread(&Player::reading_thread, this).detach();
}

void Player::add_points(int p) {
    this->current_score += p;
}

int Player::get_timeout() const {
    return this->timeout;
}


int Player::get_current_score() const {
    return this->current_score;
}



std::unique_lock<std::mutex> Player::get_connection_lock() {
    return std::unique_lock<std::mutex>(this->connection_mutex);
}

std::unique_lock<std::mutex> Player::get_my_turn_lock() {
    return std::unique_lock<std::mutex>(this->my_turn_mutex);
}

std::unique_lock<std::mutex> Player::get_write_lock() {
    return std::unique_lock<std::mutex>(this->write_mutex);
}




void Player::set_socket_fd(int fd) {
    this->socket_fd = fd;
}

void Player::notify_connection() {
    this->connection_cv.notify_all();
}



Position Player::get_pos() const {
    return this->position;
}

void Player::print_hand() {
    for (const auto &card: this->hand) {
        card.print_card();
        std::cout << " ";
    }
    std::cout << std::endl;
}


// Returns 0 if sending was successful, -1 if connection was closed, -2 if error occurred
int Player::send_deal() {
    std::stringstream ss;
    ss << "DEAL";
    Round *r = this->current_round;
    ss << r->get_round_type();
    ss << position_no_to_char.at(static_cast<int>(r->get_starting_player()));
    ss << r->get_starting_hand(this->position);
    std::string s = ss.str();
    size_t length = s.size();
    while (true) {
        std::unique_lock<std::mutex> lock(this->write_mutex);
        ssize_t managed_to_write = writen(this->socket_fd, s.c_str(), length);
        if (managed_to_write < length) { //
            std::unique_lock<std::mutex> lock_conn(this->connection_mutex);
            this->connected = false;
            return -1;
        }
        return 0;
    }
}

//TODO moze uzyc?
//int Player::send_taken() {
//    for (Trick t: this->current_round->get_played_tricks()) {
//        std::stringstream ss;
//        ss << "TAKEN";
//        ss << t.get_trick_number();
//        for (const auto &c: t.get_played_cards()) {
//            ss << value_to_string.at(c.get_value());
//            ss << color_to_char.at(c.get_color());
//        }
//        ss << position_to_char.at(static_cast<int>(t.get_taking_player()));
//        std::string s = ss.str();
//        size_t length = s.size();
//        ssize_t  result = writen(this->socket_fd, s.c_str(), length);
//        if (result < length) {
//            std::unique_lock<std::mutex> lock_conn(this->connection_mutex);
//            this->connected = false;
//            return -1;
//        }
//    }
//    return 0;
//}

int Player::send_taken(Trick& t) {
    std::stringstream ss;
        ss << "TAKEN";
        ss << t.get_trick_number();
        for (const auto &c: t.get_played_cards()) {
            ss << value_to_string.at(c.get_value());
            ss << color_to_char.at(c.get_color());
        }
        ss << position_no_to_char.at(static_cast<int>(t.get_taking_player()));
    std::string s = ss.str();
        size_t length = s.size();
        ssize_t  result = writen(this->socket_fd, s.c_str(), length);
        if (result < length) {
            std::unique_lock<std::mutex> lock_conn(this->connection_mutex);
            this->connected = false;
            return -1;
        }
        return 0;
}




void Player::set_current_round(Round *r) {
    this->current_round = r;
}

Round *Player::get_current_round() const {
    return this->current_round;
}

void Player::set_current_trick(Trick *t) {
    this->current_trick = t;
}

