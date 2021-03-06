#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <cerrno>
#include <cstdarg>
#include <string>
#include <iostream>

/* buffer for reading from tun/tap interface, must be >= 1500 */
#define BUFSIZE 2000
#define PORT 55555

int debug = 0;
char *progname;
int sock_fd;
int tap_fd;

/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            must reserve enough space in *dev.                          *
 **************************************************************************/
int tun_alloc(std::string &dev, short flags) {

    ifreq ifr{};
    int fd, err;
    const char *clonedev = "/dev/net/tun";

    if ((fd = open(clonedev, O_RDWR)) < 0) {
        perror("Opening /dev/net/tun");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = flags;

    if (!dev.empty()) {
        strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ);
    }

    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        perror("ioctl(TUNSETIFF)");
        close(fd);
        return err;
    }

    dev = ifr.ifr_name;

    return fd;
}

/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
ssize_t cread(int fd, char *buf, size_t n) {

    ssize_t nread;

    if ((nread = read(fd, buf, n)) < 0) {
        perror("Reading data");
        exit(1);
    }
    return nread;
}

/**************************************************************************
 * cwrite: write routine that checks for errors and exits if an error is  *
 *         returned.                                                      *
 **************************************************************************/
ssize_t cwrite(int fd, char *buf, size_t n) {

    ssize_t nwrite;

    if ((nwrite = write(fd, buf, n)) < 0) {
    perror("Writing data");
    exit(1);
    }
    return nwrite;
}

/**************************************************************************
 * read_n: ensures we read exactly n bytes, and puts them into "buf".     *
 *         (unless EOF, of course)                                        *
 **************************************************************************/
ssize_t read_n(int fd, char *buf, size_t n) {

    ssize_t nread;
    size_t left = n;

    while (left > 0) {
        if ((nread = cread(fd, buf, left)) == 0) {
            return 0;
        } else {
            left -= nread;
            buf += nread;
        }
    }
    return n;
}

int connect_to_server(const std::string &ifname, const std::string &remote_ip) {
    short flags = IFF_TUN;
    std::string if_name = ifname;
    int maxfd;
    ssize_t nread, nwrite, plength;
    char buffer[BUFSIZE];
    sockaddr_in remote{};
    unsigned short int port = PORT;
    unsigned long int tap2net = 0, net2tap = 0;

    /* initialize tun/tap interface */
    if ((tap_fd = tun_alloc(if_name, flags | IFF_NO_PI)) < 0) {
        fprintf(stderr, "Error connecting to tun/tap interface %s!\n", if_name.c_str());
        exit(1);
    }

    printf("Successfully connected to interface %s\n", if_name.c_str());

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket()");
        exit(1);
    }


    /* Client, try to connect to server */

    /* assign the destination address */
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = inet_addr(remote_ip.c_str());
    remote.sin_port = htons(port);

    /* connection request */
    if (connect(sock_fd, (struct sockaddr*) &remote, sizeof(remote)) < 0) {
        perror("connect()");
        exit(1);
    }

    int net_fd = sock_fd;
    printf("CLIENT: Connected to server %s\n", inet_ntoa(remote.sin_addr));

    /* use select() to handle two descriptors at once */
    maxfd = (tap_fd > net_fd) ? tap_fd : net_fd;

    while (true) {
        int ret;
        fd_set rd_set;

        FD_ZERO(&rd_set);
        FD_SET(tap_fd, &rd_set);
        FD_SET(net_fd, &rd_set);

        ret = select(maxfd + 1, &rd_set, nullptr, nullptr, nullptr);

        if (ret < 0 && errno == EINTR) {
            continue;
        }

        if (ret < 0) {
            perror("select()");
            exit(1);
        }

        if (FD_ISSET(tap_fd, &rd_set)) {
            /* data from tun/tap: just read it and write it to the network */

            nread = cread(tap_fd, buffer, BUFSIZE);

            tap2net++;
            printf("TAP2NET %lu: Read %lu bytes from the tap interface\n", tap2net, nread);

            /* write length + packet */
            plength = htons(nread);
            nwrite = cwrite(net_fd, (char *) &plength, sizeof(plength));  // TODO(AntonyMo): implement write check
            nwrite = cwrite(net_fd, buffer, nread);

            printf("TAP2NET %lu: Written %lu bytes to the network\n", tap2net, nwrite);
        }

        if (FD_ISSET(net_fd, &rd_set)) {
            /* data from the network: read it, and write it to the tun/tap interface.
            * We need to read the length first, and then the packet */

            /* Read length */
            nread = read_n(net_fd, (char *) &plength, sizeof(plength));
            if (nread == 0) {
                /* ctrl-c at the other end */
                break;
            }

            net2tap++;

            /* read packet */
            nread = read_n(net_fd, buffer, ntohl(plength));
            printf("NET2TAP %lu: Read %lu bytes from the network\n", net2tap, nread);

            /* now buffer[] contains a full packet or frame, write it into the tun/tap interface */
            nwrite = cwrite(tap_fd, buffer, nread);
            printf("NET2TAP %lu: Written %lu bytes to the tap interface\n", net2tap, nwrite);
        }
    }

    printf ("Simpletun terminated\n");
    return (0);
}
