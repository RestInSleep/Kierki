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
#include <queue>
#include <unordered_map>
#include <functional>
#include "cards.h"
#include "player.h"



#define QUEUE_LENGTH 5
#define NO_OF_PLAYERS 4
#define MAX_HAND_SIZE 13
#define MAX_MESSAGE_SIZE 128
#define DEFAULT_TIMEOUT 5


extern const std::unordered_map<int, char>  position_no_to_char;
extern const std::unordered_map<int, char>  char_to_position_no;


bool check_IAM_message(const char* buffer, ssize_t length_read);
int check_TRICK_from_client(const char* buffer, ssize_t length_read);

uint16_t read_port(char const *string);

ssize_t readn(int fd, void *vptr, size_t n);

ssize_t writen(int fd, const void *vptr, size_t n);

void set_timeout(int socket_fd, uint32_t timeout);




int get_round_type_from_sett(std::string& s);

std::string get_cmd_option(char ** begin, char ** end, const std::string& option);

bool cmd_option_exists(char** begin, char** end, const std::string& option);

bool conditional_cmd_option_exists(char** begin, char** end, const std::function<bool(const std::string&)> &condition);

std::string conditional_get_cmd_option(char ** begin, char ** end, const std::function<bool(const std::string&)> &condition);

class ReportPrinter {
    std::mutex queue_mutex;
    std::queue<std::string> messages;
    std::condition_variable not_empty;
public:
    ReportPrinter();
    [[noreturn]]  void printing_thread();
    void add_message(const std::string& message);
    void add_report_log_to_client(const std::string& message, Player& p);
    void add_report_log_from_client(const std::string& message, Player& p);
    void add_report_log_to_client(const std::string& message, const std::string& server_ip, int server_port,
                                    const std::string& client_ip, int client_port);
    void add_report_log_from_client(const std::string& message, const std::string& server_ip, int server_port,
                                      const std::string& client_ip, int client_port);
};



#endif //KIERKI_COMMON_H
