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
#include <vector>
#include <unordered_map>



#define QUEUE_LENGTH 5
#define NO_OF_PLAYERS 4
#define MAX_HAND_SIZE 13


extern std::mutex g_number_of_players_mutex;
extern const std::unordered_map<int, char>  position_to_char;
extern const std::unordered_map<int, char>  char_to_position;







bool check_IAM_message(const char* buffer, ssize_t length_read);
int check_TRICK_from_client(const char* buffer, ssize_t length_read);

uint16_t read_port(char const *string);

ssize_t readn(int fd, void *vptr, size_t n);

ssize_t writen(int fd, const void *vptr, size_t n);

void set_timeout(int socket_fd, uint32_t timeout);




int get_round_type_from_sett(std::string& s);



#endif //KIERKI_COMMON_H
