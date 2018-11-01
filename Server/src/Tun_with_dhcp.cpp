//
// Created by antonymo on 03.05.18.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <map>
#include <sstream>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <algorithm>
#include <thread>
#include <set>

#include "../include/rwr.hpp"
#include "../include/dhcp_errors.h"
#include "../include/Network.h"

#define DHCP_BUFSIZE 4096
#define BUFSIZE 20000
#define DHCP_PORT 12345
#define PORT 55555

int debug = 1;

int shut = false;

std::map<std::string, Network*> network_list;
std::set<int> clients;

int foo() {
    int dhcp_s = socket(AF_INET, SOCK_STREAM, 0);
    in_addr in;
    int res = inet_aton("0.0.0.0", &in);
    if (!res) {
        return -2;
    }

    sockaddr_in sockaddr_ = {
            .sin_family = AF_INET,
            .sin_port = htons(DHCP_PORT),
            .sin_addr = in
    };

    res = bind(dhcp_s, (sockaddr*) &sockaddr_, sizeof(sockaddr_));
    if (res) {
        std::cout << std::strerror(errno);
        return -1;
    }

    listen(dhcp_s, 5);

    unsigned short int net_number = 170;  // чтобы ip создаваемых сетей различались
    // пока что выход за 255 не обрабатывается

    sockaddr_in  peer_addr;
    socklen_t peer_addr_size;
    while(true)  {
        int ss = accept(dhcp_s, (struct sockaddr *)  &peer_addr, &peer_addr_size);

        std::string ip = inet_ntoa(peer_addr.sin_addr);

        std::cout << "Recieved request from " << ip << std::endl;

        char buff[DHCP_BUFSIZE];
        for(auto &iter: buff) {
            iter = '\0';
        }
        recv(ss, buff, DHCP_BUFSIZE, 0);
        std::stringstream input(buff);
        std::string request_type;
        std::string network;
        std::string password;

        input >> request_type;

        std::string vip;
        if (request_type == "shutdown") {
            shut = true;
            break;
        }
        if (request_type == "disconnect") {
            for (const auto &iter : network_list) {
                if(iter.second->remove_peer(ip)) {
                    std::cout << "Peer successfully removed from \"" << iter.first << "\" network." << std::endl;
                    break;
                }
            }

            continue;
        }

        input  >> network >> password;
        if(request_type == "connect") {
            std::cout << "Peer requests connection." << std::endl;
            std::cout << "Trying to connect to \"" << network << "\" network..." << std::endl;
            if(network_list.find(network) != network_list.end()) {
                vip = network_list[network]->add_peer(password, ip);
                if(!vip.empty()) {
                    if (vip == "NET_FULL") {
                        std::cout << "Connection error: network is full." << std::endl;
                        send(ss, (void *)NET_FULL, strlen(NET_FULL), 0);
                        close(ss);
                    } else {
                        std::cout << "Peer successfully added." << std::endl;
                        std::cout << "Generated VIP is: " << vip << std::endl;
                        std::cout << "Returning vip to the peer." << std::endl;
                        send(ss, (void *) vip.c_str(), vip.size(), 0);
                        close(ss);
                    }
                } else {
                    std::cout << "Connection error: password incorrect." << std::endl;
                    send(ss, (void *)PASSWD_INC, strlen(PASSWD_INC), 0);
                    close(ss);
                }
            } else {
                std::cout << "Connection error: network doesn't exist." << std::endl;
                send(ss, (void *)NET_NAME_INC, strlen(NET_NAME_INC), 0);
                close(ss);
            }
        } else {
            std::cout << "Peer requests network creation." << std::endl;
            if(network_list.find(network) != network_list.end()) {
                std::cout << "Network creation error: network \"" << network << "\" already exists." << std::endl;
                send(ss, (void *)NET_NAME_TAKEN, strlen(NET_NAME_TAKEN), 0);
                close(ss);
            } else {
                network_list[network] = new Network(network, password, "170." + std::to_string(net_number++) + ".0.");
                std::cout << "Network \"" << network << "\" successfully created." << std::endl;
                vip = network_list[network]->add_peer(password, ip);
                std::cout << "Generated VIP is: " << vip << std::endl;
                std::cout << "Returning vip to the peer." << std::endl;
                send(ss, (void *) vip.c_str(), vip.size(), 0);
                close(ss);
            }
        }

        std::cout << std::endl;

        input.clear();
    }

    for (auto iter: network_list) {
        delete iter.second;
    }
}

int main() {
    char buffer[BUFSIZE];
    sockaddr_in local, remote;
    unsigned short int port = PORT;
    int sock_fd, optval = 1;
    socklen_t remotelen;
    unsigned long int tap2net = 0;

    if ( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(1);
    }


    if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
        perror("setsockopt()");
        exit(1);
    }

    fcntl(sock_fd, F_SETFL, O_NONBLOCK);
    int net_fd;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);
    if (bind(sock_fd, (struct sockaddr*) &local, sizeof(local)) < 0) {
        perror("bind()");
        exit(1);
    }
    my_err((char*)"SERVER OPENED\n");
    if (listen(sock_fd, 5) < 0) {
        perror("listen()");
        exit(1);
    }

    clients.clear();


    std::thread th(foo);


    while(!shut){
        /* wait for connection request */
        remotelen = sizeof(remote);
        memset(&remote, 0, remotelen);
        fd_set readset;
        uint16_t nread, nwrite, plength;
        FD_ZERO(&readset);
        FD_SET(sock_fd, &readset);
        for(auto it: clients) {
            FD_SET(it, &readset);
        }

        int mx;
        if (clients.size() > 0) {
            mx = std::max(sock_fd, *max_element(clients.begin(), clients.end()));
        } else {
            mx = sock_fd;
        }

        if(select(mx+1, &readset, NULL, NULL, NULL) <= 0)
        {
            perror("select");
            exit(3);
        }


        if(FD_ISSET(sock_fd, &readset)) {

            if ((net_fd = accept(sock_fd, (struct sockaddr*)&remote, &remotelen)) < 0) {
                perror("accept()");
                exit(1);
            }
            do_debug(debug, (char*)"SERVER: Client connected from %s\n", inet_ntoa(remote.sin_addr));

            fcntl(net_fd, F_SETFL, O_NONBLOCK);

            for (const auto &iter : network_list) {
                if(iter.second->try_connect_peer(inet_ntoa(remote.sin_addr), net_fd)) {
                    std::cout << "Client added to the \"" << iter.first << "\" network." << std::endl;
                    break;
                }
            }

            clients.insert(net_fd);
        }
        for(auto it: clients) {
            if(FD_ISSET(it, &readset)) {
                nread = read_n(it, (char *)&plength, sizeof(plength));
                if(nread == 0) {
                    /* ctrl-c at the other end */
                    break;
                }
                tap2net++;

                /* read packet */
                nread = read_n(it, (char *)buffer, ntohs(plength));
                std::cout << "TAP2NET "<< tap2net <<": Read " << nread << " bytes from the network" << it << "\n";
                std::string destination = std::to_string((int(buffer[16]) + 256) % 256) + "." + std::to_string((int(buffer[17]) + 256) % 256) +"." + std::to_string((int(buffer[18]) + 256) % 256) + "." + std::to_string((int(buffer[19]) + 256) % 256);
                std::cout << "destination: " << destination << std::endl;


                for (const auto &iter : network_list) {
                    if(iter.second->try_reroute_package(destination, buffer, nread)) {
                        std::cout << "TAP2NET "<< tap2net <<": Written " << nread << " bytes to the network\n";
                        break;
                    }
                }
            }
        }
    }

    th.join();

    close(sock_fd);

    for(auto it: clients) {
        close(it);
    }

    return 0;
}