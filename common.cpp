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
#include <iomanip>
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


std::string getFormattedTimestamp() {
    using namespace std::chrono;

    auto now = system_clock::now();

    auto now_time_t = system_clock::to_time_t(now);

    auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm now_tm = *std::gmtime(&now_time_t);

    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << now_ms.count();

    return oss.str();
}



ReportPrinter::ReportPrinter() {
    std::thread t(&ReportPrinter::printing_thread, this);
    t.detach();
}

void ReportPrinter::add_report_log_to_client(const std::string &message, Player &p) {
    std::string log = "[";
    log += p.get_server_interface_ip();
    log += ":";
    log += std::to_string(p.get_server_port());
    log += ",";
    log += p.get_client_ip();
    log += ":";
    log += std::to_string(p.get_client_port());
    log += ",";
    log += getFormattedTimestamp();
    log += "] ";
    log += message;
    add_message(log);
}

void ReportPrinter::add_report_log_from_client(const std::string &message, Player &p) {

    std::string log = "[";
    log += p.get_client_ip();
    log += ":";
    log += std::to_string(p.get_client_port());
    log += ",";
    log += p.get_server_interface_ip();
    log += ":";
    log += std::to_string(p.get_server_port());
    log += ",";
    log += getFormattedTimestamp();
    log += "] ";
    log += message;
    add_message(log);
}

void ReportPrinter::add_report_log_to_client(const std::string &message, const std::string& server_ip, int server_port,
                                               const std::string& client_ip, int client_port) {
    std::string log = "[";
    log += server_ip;
    log += ":";
    log += std::to_string(server_port);
    log += ",";
    log += client_ip;
    log += ":";
    log += std::to_string(client_port);
    log += ",";
    log += getFormattedTimestamp();
    log += "] ";
    log += message;
    add_message(log);
}

void ReportPrinter::add_report_log_from_client(const std::string &message, const std::string& server_ip, int server_port,
                                                 const std::string& client_ip, int client_port) {
    std::string log = "[";
    log += client_ip;
    log += ":";
    log += std::to_string(client_port);
    log += ",";
    log += server_ip;
    log += ":";
    log += std::to_string(server_port);
    log += ",";
    log += getFormattedTimestamp();
    log += "] ";
    log += message;
    add_message(log);
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
