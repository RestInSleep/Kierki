//
// Created by jan on 26/05/24.
//

#include "player.h"
#include "err.h"
#include <iostream>
#include <sstream>

int Player::number_of_connected_players = 0;

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

void Player::start_reading_thread() {
    std::thread(&Player::reading_thread, this).detach();
}

void Player::add_points(int p) {
    this->current_score += p;
}

int Player::get_timeout() const {
    return this->timeout;
}

void Player::set_timeout(int t) {
    this->timeout = t;
}

int Player::get_current_score() const {
    return this->current_score;
}

void Player::add_connected_player() {
    std::lock_guard<std::mutex> lock(g_number_of_players_mutex);
    Player::number_of_connected_players++;
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
    //TODO
    std::stringstream ss;
    ss << "DEAL";
    Round * r = this->current_round;
    ss <<  r->get_round_type();
    ss << position_to_char.at(static_cast<int>(r->get_starting_player()));
    ss << r->get_starting_hand(this->position);
    std::string s = ss.str();
    size_t length = s.size();
    size_t sent = 0;
    while (sent < length) {
        ssize_t snd = send(this->socket_fd, s.c_str() + sent, length - sent, 0);
        if (snd < 0) {
            return -2;
        }
        if (snd == 0) {
            return -1;
        }
        sent += snd;
    }
    return 0;
}



