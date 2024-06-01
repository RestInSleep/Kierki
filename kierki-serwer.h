//
// Created by jan on 18/05/24.
//

#ifndef KIERKI_KIERKI_SERWER_H
#define KIERKI_KIERKI_SERWER_H


#include "player.h"
#define MAX_ACCEPTING_THREADS 4
class Options {
    uint16_t port;
    std::string filename;
    uint32_t timeout;
public:
    Options();
    void set_port(uint16_t p);
    void set_filename(const std::string& f);
    void set_timeout(uint32_t t);
    [[nodiscard]] uint16_t get_port() const;
    [[nodiscard]] std::string get_filename() const;
    [[nodiscard]] uint32_t get_timeout() const;
};

bool all_players_connected(const std::vector<Player>& players);

void wait_for_all_players(Player* players);

void send_busy(int connection_fd, Player *players, int pos);




#endif //KIERKI_KIERKI_SERWER_H
