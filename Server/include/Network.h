//
// Created by antonymo on 03.05.18.
//

#ifndef SERVER_NETWORK_H
#define SERVER_NETWORK_H

#include <map>

class Network {
public:
    explicit Network(const std::string &name, const std::string &pass, const std::string &vip_base)
            : name(name), password(pass), vip_base(vip_base) {
    }
    std::string add_peer(const std::string &pass, const std::string &ip);
    bool try_connect_peer(const std::string &ip, int fd);
    bool try_reroute_package(const std::string &vip, char *buffer, int size);
    bool remove_peer(const std::string &ip);

private:
    const std::string name;
    const std::string password;
    unsigned short int MAX_NUM = 254;
    unsigned short int ip_number = 2;
    unsigned short int ip_count = 0;
    const std::string vip_base;
    std::map<std::string, std::string> vip_table;
    std::map<std::string, int> sock_table;
    std::string generate_vip();
};


#endif //SERVER_NETWORK_H
