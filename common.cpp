//
// Created by jan on 18/05/24.
//
#include <cinttypes>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <regex>
#include <sys/socket.h>
#include <string_view>
#include <thread>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include "err.h"
#include "cards.h"
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

// Returns 0 if the message is not a correct prefix
// Returns 1 if the message is a correct prefix
// Returns 2 if the message is a correct TRICK mess,age

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
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
           std::cout << "nwritten: " << nwritten << std::endl;
            return nwritten;  // error
        }
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

int get_round_type_from_sett(std::string &s) {
    size_t length = s.size();
    std::string round_type = s.substr(0, length - 1);
    return std::stoi(round_type);
}


std::string get_cmd_option(char ** begin, char ** end, const std::string& option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        std::string str = *itr;
        return str;
    }
    fatal("Option %s was not given a value despite being mentioned", option.c_str());
}

bool cmd_option_exists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}


bool conditional_cmd_option_exists(char** begin, char** end, const std::function<bool(const std::string&)> &condition) {
    return std::find_if(begin, end, condition) != end;
}

std::string conditional_get_cmd_option(char ** begin, char ** end, const std::function<bool(const std::string&)> &condition) {
    char ** itr = std::find_if(begin, end, condition);
    if (itr != end && ++itr != end)
    {
        std::string str = *itr;
        return str;
    }
    fatal("Option was not given a value despite being mentioned");
}



ReportPrinter::ReportPrinter() {
    std::thread t(&ReportPrinter::printing_thread, this);
    t.detach();
}

 void ReportPrinter::printing_thread() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (messages.empty()) {
            not_empty.wait(lock);
        }
        if (messages.empty()) {
            continue;
        }
        std::string message = messages.front();
        messages.pop();
        lock.unlock();
        std::cout << message;
    }
}

void ReportPrinter::add_message(const std::string &message) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        messages.emplace(message);
    }
    not_empty.notify_all();
}
