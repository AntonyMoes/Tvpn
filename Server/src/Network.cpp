//
// Created by antonymo on 03.05.18.
//

#include <cstring>
#include <netinet/in.h>
#include <set>
#include "../include/Network.h"
#include "../include/rwr.hpp"

extern std::set<int> clients;


std::string Network::generate_vip() {
    if (ip_count == MAX_NUM) {
        return "NET_FULL";
    }

    while (!vip_table[vip_base  + std::to_string(ip_number)].empty()) {
        if (ip_number == MAX_NUM + 1) {
            ip_number = 2;
        } else {
            ip_number++;
        }

    }

    ip_count++;

    return vip_base + std::to_string(ip_number);
}
std::string Network::add_peer(const std::string &pass, const std::string &ip) {
    if (pass != password) {
        return "";
    }
    std::string vip = generate_vip();
    vip_table[vip] = ip;



    return vip;
}

bool Network::try_connect_peer(const std::string &ip, int fd) {
    for (const auto &iter: vip_table) {
        if(ip == iter.second) {
            sock_table[iter.first] = fd;
            return true;
        }
    }

    return false;
}

bool Network::try_reroute_package(const std::string &vip, char *buffer, int size) {
    if(strncmp(vip.c_str(), vip_base.c_str(), vip_table.size() - 1)) {
        return false;
    }

    for (const auto &iterator: sock_table) {
        if (vip == iterator.first) {
            uint16_t plength = htons(size);
            cwrite(iterator.second, (char *) &plength, sizeof(plength));
            cwrite(iterator.second, buffer, size);
            return true;
        }
    }
    return false;
}

bool Network::remove_peer(const std::string &ip) {
    for (const auto &iter: vip_table) {
        if(ip == iter.second) {
            clients.erase(sock_table[iter.first]);
            close(sock_table[iter.first]);
            sock_table.erase(iter.first);
            vip_table.erase(iter.first);

            ip_count--;


            return true;
        }
    }
}
