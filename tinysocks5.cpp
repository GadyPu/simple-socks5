#include<sys/select.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<arpa/inet.h>
#include<signal.h>
#include<unistd.h>
#include<netdb.h>
#include<fcntl.h>
#include<getopt.h>
#include<cstdio>
#include<ctime>
#include<cstdlib>
#include<cstring>
#include<cerrno>
#include"io_buffer.h"
#include"logger.h"
#include"cipher.h"

#pragma pack(4)
#define MAX_EVENTS 1024
#define BUFF_SIZE 2048

Cipher *cipher = nullptr;
bool EP_ET_ON = true;
Logger logger(Logger::File_And_Terminal, Logger::Info, "s5server.log");

enum SocksState {
    ERROR = -1,
    AUTHENTICATE = 0,
    CONNECTING = 1,
    FORWARDING = 2
};
struct event_data {
    IoBuffer* sendbuf;
    IoBuffer* recvbuf;
    void *pairptr;
    int fd;
    time_t activity;
    SocksState status;
    bool writestate;  // false send data; true send later
    bool is_client;  // true client; false remote
    bool pairclosed;
    event_data() :is_client(true), status(AUTHENTICATE), writestate(false),
            fd(-1), pairptr(nullptr), pairclosed(false) {
        activity = time(nullptr);
        sendbuf = new IoBuffer(BUFF_SIZE);
        recvbuf = new IoBuffer(BUFF_SIZE);
    }
    ~event_data() {
        pairptr = nullptr;
        if (sendbuf) {
            delete sendbuf;
        }
        if (recvbuf) {
            delete recvbuf;
        }
        sendbuf = recvbuf = nullptr;
    }
};

void setNoBlocking(int fd) {
    int flag = fcntl(fd, F_GETFL);
    if (flag < 0) {
        logger.ERRORS("set socket NoBlocking error");
        exit(1);
    }
    flag |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flag) < 0) {
        logger.ERRORS("set socket NoBlocking error");
        exit(1);
    }
}

event_data* handle_epoll_add(int epfd, int fd, uint32_t event, bool is_client) {
    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    event_data *ptr = new event_data();
    if (!ptr) {
        logger.ERRORS("in function \'handle_epoll_add\' malloc error");
        return nullptr;
    }
    ptr->fd = fd;
    ptr->is_client = is_client;
    ev.events = event;
    ev.data.ptr = ptr;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        logger.ERRORS("in function \'handle_epoll_add\' EPOLL_CTL_ADD error");
        if (ptr) {
            delete ptr;
            ptr = nullptr;
        }
        return nullptr;
    }
    return ptr;
}

void handle_accept(int epfd, int fd) {

    int con_sock = -1;
    struct sockaddr_in addr;
    bzero(&addr, 0);
    addr.sin_family = AF_INET;
    socklen_t sock_len = sizeof(struct sockaddr_in);
    while ((con_sock = accept(fd, (struct sockaddr *) &addr, &sock_len)) > 0) {
        setNoBlocking(con_sock);
        handle_epoll_add(epfd, con_sock, EPOLLIN | EPOLLRDHUP | EPOLLERR | (EP_ET_ON ? EPOLLET : 0), true);
    }
    if (con_sock == -1) {
        if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
            logger.WARNING("in function \'handle_accept\' accept error");
            return;
        }
    }
}

void handle_epoll_mod(int epfd, event_data *&data_ptr, uint32_t event) {
    if (!data_ptr) {
        return;
    }
    struct epoll_event ev;
    bzero(&ev, 0);
    ev.data.ptr = data_ptr;
    ev.events = event;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, data_ptr->fd, &ev) < 0) {
        logger.WARNING("in function \'handle_epoll_mod\' EPOLL_CTL_MOD error");
    }
}

void handle_epoll_remove_fd(int epfd, event_data *&data_ptr) {
    if (!data_ptr || data_ptr->fd <= 0) {
        return;
    }
    epoll_ctl(epfd, EPOLL_CTL_DEL, data_ptr->fd, nullptr);
    if (close(data_ptr->fd) != 0) {
        logger.INFO("in function \'handle_epoll_remove_fd\' close fd error");
        return;
    }
    data_ptr->fd = -1;
    if (data_ptr->pairptr != nullptr) {
        struct event_data *pair_ptr = (struct event_data *)data_ptr->pairptr;
        if (pair_ptr->sendbuf->readableBytes() > 0) {
            pair_ptr->pairclosed = true;
        } else {
            handle_epoll_mod(epfd, pair_ptr , EPOLLRDHUP | EPOLLERR);
        }
        ((struct event_data *)data_ptr->pairptr)->pairptr = nullptr;
    }

    delete data_ptr;
    data_ptr = nullptr;
}

int recv_data(int epfd, event_data *&data_ptr) {
    IoBuffer *&recv_buf = data_ptr->recvbuf;
    char temp_buf[BUFF_SIZE] = {0};
    int nread = 0;
    if (data_ptr->fd <= 0) {
        logger.WARNING("in function \'recv_data\' data_ptr->fd less than zero");
        return 1;
    }
    while ((nread = read(data_ptr->fd, temp_buf, sizeof(temp_buf))) > 0) {
        if (cipher && data_ptr->is_client) {
            cipher->decrypt_data(temp_buf, nread);
        }
        recv_buf->append(temp_buf, nread);
    }
    if (nread == -1 && errno != EAGAIN) {
        handle_epoll_remove_fd(epfd, data_ptr);
        logger.INFO("in function \'recv_data\' recv data error");
        return -1;
    }
    if (nread == 0) {
        return 1;
    } else {
        return 0;
    }
}

void send_data(int epfd, event_data *&data_ptr) {
    if (data_ptr && data_ptr->writestate == false) {
        if (data_ptr->sendbuf->readableBytes() > 0) {
            int nwrite, data_size = data_ptr->sendbuf->readableBytes();
            int n = data_size;
            const char *buf = data_ptr->sendbuf->peek();
            while (n > 0) {
                nwrite = write(data_ptr->fd, buf + data_size - n, n);
                if (nwrite < n) {
                    if (nwrite == -1 && errno != EAGAIN) {
                        handle_epoll_remove_fd(epfd, data_ptr);
                        logger.INFO("in function \'send_data\' send data error");
                        return;
                    }
                    if (errno == EAGAIN) {
                        if (nwrite != -1) {
                            n -= nwrite;
                        }
                        handle_epoll_mod(epfd, data_ptr, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLRDHUP | (EP_ET_ON ? EPOLLET : 0));
                        data_ptr->writestate = true;
                    }
                    break;
                }
                n -= nwrite;
            }
            if (n == 0) {
                data_ptr->sendbuf->seekReadIndex(data_size);
                if (data_ptr->pairclosed == true) {
                    handle_epoll_remove_fd(epfd, data_ptr);
                }
            } else {
                data_ptr->sendbuf->seekReadIndex(data_size - n);
            }
        }
    }
}

bool handle_socks_connect(int epfd, event_data *&data_ptr) {
    if (!data_ptr) {
        return false;
    }
    char ip[24] = {0};
    char port[2] = {0};
    char host_name[256] = {0};
    char replay[10] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    SocksState *status = (SocksState *)&data_ptr->status;
    IoBuffer *&recv_buf = data_ptr->recvbuf;
    IoBuffer *&send_buf = data_ptr->sendbuf;
    if (*status == AUTHENTICATE) {
        if (recv_buf->readableBytes() >= 3) {
            recv_buf->seekReadIndex(3);
            if (cipher) {
                cipher->encrypt_data(replay, 2);
            }
            send_buf->append(replay, 2);
            *status = CONNECTING;
        } else {
            *status = ERROR;
        }
    } else if (*status == CONNECTING) {
        struct hostent *hostptr = nullptr;
        const char *buf = recv_buf->peek();
        if ((recv_buf->readableBytes() > 4) && buf[1] == 0x01) {
            switch (buf[3]) {
                case 0x01:
                    memcpy(ip, buf + 4, 4);
                    memcpy(port, buf + 4 + 4, 2);
                    recv_buf->seekReadIndex(10);
                    break;
                case 0x03:
                    memcpy(host_name, buf + 5, buf[4]);
                    memcpy(port, buf + 5 + buf[4], 2);
                    hostptr = gethostbyname(host_name);
                    if (hostptr) {
                        memcpy(ip, hostptr->h_addr_list[0], hostptr->h_length);
                        logger.INFO("connect to host name: " + std::string(host_name));
                        recv_buf->seekReadIndex(4 + 1 + buf[4] + 2);
                    } else {
                        *status = ERROR;
                    }
                    break;
                case 0x04:
                    *status = ERROR;
                    printf("ipv6 not support\n");
                default:
                    break;
            };
        } else {
            *status = ERROR;
        }
        if (*status != ERROR) {
            if (cipher) {
                cipher->encrypt_data(replay, 10);
            }
            send_buf->append(replay, 10);
            int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (remote_fd < 0) {
                logger.ERRORS("cannot create remote socket");
                *status = ERROR;
            }
            struct sockaddr_in addr;
            bzero(&addr, 0);
            addr.sin_family = AF_INET;
            memcpy(&addr.sin_addr.s_addr, ip, 4);
            memcpy(&addr.sin_port, port, 2);
            // printf("reomte: %s, port: %d.\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            bzero(host_name, sizeof(host_name));
            sprintf(host_name, "connect to reomte addr %s port: %d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            logger.INFO(std::string(host_name));
            setNoBlocking(remote_fd);
            connect(remote_fd, (const sockaddr *)&addr, sizeof(addr));

            event_data *opt = handle_epoll_add(epfd, remote_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLRDHUP | (EP_ET_ON ? EPOLLET : 0), false);
            if (opt) {
                data_ptr->pairptr = opt;
                opt->pairptr = data_ptr;
                *status = FORWARDING;
            } else {
                *status = ERROR;
            }
        }
    } else if (*status == FORWARDING) {
        event_data *pair_ptr = (event_data *)data_ptr->pairptr;
        if (!pair_ptr) {
            return false;
        }
        std::string temp = data_ptr->recvbuf->retriveAllString();
        pair_ptr->sendbuf->append(temp.c_str(), temp.size());
        send_data(epfd, pair_ptr);
    } else if (*status == ERROR) {
        replay[1] = 0x05;
        if (cipher) {
            cipher->encrypt_data(replay, 10);
        }
        send_buf->append(replay, 10);
        send_data(epfd, data_ptr);
        logger.INFO("socks5 handshake failed try again");
        return false;
    }
    return true;
}

static void sigign() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, 0);
}

void usage(const char * name) {
    printf(
        "A simple socks5 proxy tool help you bypass firewall.\n"
        "awesome c/c++ linux\n"
        "author: funny boy ^_^\n"
        "usage: %s [options]\n"
        "options: \n"
        "  -a <address>         Local Address to bind (default: 0.0.0.0).\n"
        "  -p <port>            Port number to bind (default: 9527).\n"
        "  -k [optional]        use simple algorithm to encrypt transform data.\n"
        "  -g [optional]        generate key file.\n"
        "  -o [optional]        turn off EPOLLET mod.\n"
        "  -v                   show this programm version.\n"
        "  -h                   Show this help message.\n", name);
}

void getOptArgs(int argc, char **argv, in_addr_t &addr, in_addr_t &port) {
    int opt;
    if (argc == 1) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
        return;
    }
    while((opt = getopt(argc, argv, "a:p:kgovh")) != -1) {
        switch (opt) {
            case 'a':
                addr = inet_addr(optarg);
                break;
            case 'p':
                port = htons(atoi(optarg));
                break;
            case 'k':
                cipher = new Cipher("./key.bin");
                break;
            case 'g':
                if (!cipher) {
                    cipher = new Cipher();
                }
                cipher->gen_key_file();
                exit(EXIT_SUCCESS);
            case 'o':
                EP_ET_ON = false;
                break;
            case 'v':
                printf("tinysocks5 v1.0\n");
                exit(EXIT_SUCCESS);
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            case '?':
                printf("Invalid argument: -%c\n", optopt);
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv) {

    sigign();
    in_addr_t addr = INADDR_ANY;
    in_addr_t port = htons(9527);
    getOptArgs(argc, argv, addr, port);
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in local_addr, remote;
    int epfd, server_fd;
    unsigned long int opt = 1;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        logger.ERRORS("cannot create local socket");
        exit(1);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, (socklen_t)(sizeof(opt)))) {
        logger.ERRORS("setsockopt reuseaddr failed!");
        exit(1);
    }
    setNoBlocking(server_fd);
    bzero(&local_addr, 0);
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = addr;
    local_addr.sin_port = port;
    if (bind(server_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        logger.ERRORS("bind server socket error");
        exit(1);
    }

    if (listen(server_fd, 1024) < 0) {
        logger.ERRORS("server listen error");
        exit(1);
    }

    epfd = epoll_create(MAX_EVENTS);
    if (epfd < 0) {
        logger.ERRORS("epoll_create error");
        exit(1);
    }

    if (handle_epoll_add(epfd, server_fd, EPOLLIN | (EP_ET_ON ? EPOLLET : 0), false) == nullptr) {
        exit(1);
    }
    char buf[256] = {0};
    if (!cipher) {
        sprintf(buf, "[+]start proxy server without encyrpt listen at %s port %d in %s mod", \
        inet_ntoa(local_addr.sin_addr), ntohs(port), (EP_ET_ON ? "EPOLLET" : "EPOLLIN"));
    } else {
        sprintf(buf, "[+]start proxy server use encyrpt method encyrpt listen at %s port %d in %s mod", \
        inet_ntoa(local_addr.sin_addr), ntohs(port), (EP_ET_ON ? "EPOLLET" : "EPOLLIN"));
    }
    logger.INFO(buf);
    while (true) {
        int ndfs = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (ndfs < 0) {
            if (errno != EINTR) {
                logger.ERRORS("epoll wait error");
                exit(1);
            } else {
                continue;
            }
        }
        for (int i = 0; i < ndfs; i++) {

            event_data *data_ptr = (event_data *)events[i].data.ptr;
            uint32_t event = events[i].events;
            int fd = data_ptr->fd;
            if (fd == server_fd) {
               handle_accept(epfd, fd);
               continue;
            }
            if (event & EPOLLIN) {
                int code = recv_data(epfd, data_ptr);
                if (code < 0 || !data_ptr) {
                    continue;
                } else if (code == 1) {
                    handle_epoll_remove_fd(epfd, data_ptr);
                } else {
                    if (data_ptr->is_client) {
                        if (!handle_socks_connect(epfd, data_ptr)) {
                            handle_epoll_remove_fd(epfd, data_ptr);
                            continue;
                        }
                        send_data(epfd, data_ptr);
                    } else {
                        event_data *pair_ptr = (event_data *)data_ptr->pairptr;
                        if (!pair_ptr) {
                            continue;
                        }
                        std::string temp = data_ptr->recvbuf->retriveAllString();
                        if (cipher) {
                            cipher->encrypt_data(temp);
                        }
                        pair_ptr->sendbuf->append(temp.c_str(), temp.size());
                        send_data(epfd, pair_ptr);
                    }
                }
            }
            if (event & EPOLLOUT) {
                if (!data_ptr) {
                    continue;
                }
                handle_epoll_mod(epfd, data_ptr, EPOLLIN | EPOLLRDHUP | EPOLLERR | (EP_ET_ON ? EPOLLET : 0));
                data_ptr->writestate = false;
                send_data(epfd, data_ptr);
            }
            if ((event & EPOLLRDHUP) || (event & EPOLLERR)) {
                handle_epoll_remove_fd(epfd, data_ptr);
            }
        }
    }
    return 0;
}
