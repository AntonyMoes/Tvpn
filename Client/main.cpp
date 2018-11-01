#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <map>
#include <sstream>
#include <thread>
#include <csignal>
#include <sys/stat.h>


const char *SERVER_ADDR = "185.17.122.182";  // mb std::string
const int SERVER_PORT = 12345;
const int BUFF_SIZE = 4* 1024;
const char *tun_name = "vpn_tun";
extern int sock_fd;
extern int tap_fd;

int connect_to_server(const std::string &ifname, const std::string &remote_ip);

void create_tun(const std::string &vip) {
    std::string syscall = "./tun.sh ";
    syscall += tun_name;
    syscall += " ";
    syscall += vip;
    system(syscall.c_str());
}

void delete_tun() {
    std::string syscall = "./tun_del.sh ";
    syscall += tun_name;
    system(syscall.c_str());
}

void term_handler(int i){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    in_addr in;
    int res = inet_aton(SERVER_ADDR, &in);
    if (!res) {
        exit;
    }

    sockaddr_in sockaddr_ = {
            .sin_family = AF_INET,
            .sin_port = htons(SERVER_PORT),
            .sin_addr = in
    };
    connect(s, (sockaddr*) &sockaddr_, sizeof(sockaddr_));
    send(s, (void *)"disconnect", strlen("disconnect"), 0);

    close(sock_fd);
    close(tap_fd);
    delete_tun();

    exit(EXIT_SUCCESS);
}

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    in_addr in;
    int res = inet_aton(SERVER_ADDR, &in);
    if (!res) {
        return -2;
    }

    sockaddr_in sockaddr_ = {
            .sin_family = AF_INET,
            .sin_port = htons(SERVER_PORT),
            .sin_addr = in
    };



    // Формирование запроса
    // -----------------------------------------

    std::string request_type;
    std::string net_name;
    std::string net_pass;

    std::cin >> request_type;

    // For developers only. This request will be removed from the public version.
    if(request_type == "shutdown") {
        connect(s, (sockaddr*) &sockaddr_, sizeof(sockaddr_));
        send(s, (void *)request_type.c_str(), strlen(request_type.c_str()), 0);
        return 0;
    } else if (request_type != "connect" && request_type != "create") {
        std::cout << "Invalid request type." << std::endl;
        return 1;
    }

    std::cin >> net_name >> net_pass;  // пока пусть будет так
    std::string request = request_type + " " + net_name + " " + net_pass;

    // -----------------------------------------



    connect(s, (sockaddr*) &sockaddr_, sizeof(sockaddr_));
    send(s, (void *)request.c_str(), strlen(request.c_str()), 0);

    char buff[BUFF_SIZE];
    int recv_bytes = recv(s, buff, BUFF_SIZE, 0);
    std::stringstream dhcp_input(buff);

    std::string vip;
    dhcp_input >> vip;

    std::cout << buff << std::endl;

    if (vip == "ERR") {
        std::string error_type;
        dhcp_input >> error_type;
        std::cout << "Error occured: " << error_type << std::endl;
        return 55;
    }
    close(s);

    create_tun(vip);

    int pid = fork();

    if (pid == -1) // если не удалось запустить потомка
    {
        // выведем на экран ошибку и её описание
        printf("Error: Start Daemon failed (%s)\n", strerror(errno));

        return -1;
    }
    else if (!pid) // если это потомок
    {
        // создаём новый сеанс, чтобы не зависеть от родителя
        setsid();

        //
        printf("My pid is %i\n", getpid());

        // закрываем дискрипторы ввода/вывода/ошибок, так как нам они больше не понадобятся
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        //
        std::signal(SIGTERM, term_handler);

        connect_to_server("vpn_tun", SERVER_ADDR);

        return 0;
    }
    else // если это родитель
    {
        // завершим процес, т.к. основную свою задачу (запуск демона) мы выполнили
        return 0;
    }
}