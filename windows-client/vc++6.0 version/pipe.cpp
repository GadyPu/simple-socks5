#include<winsock.h>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<ctime>
#include<cerrno>
#include<set>
#include"io_buffer.h"
#pragma comment(lib, "ws2_32.lib")
struct client_t {
   bool inuse;
   int c_sock, r_sock;
   time_t activity;
};

#define MAX_BUF 4096
#define MAX_CLIENTS 1024
#define IDLETIMEOUT 60

typedef unsigned char uint8_t;
struct Cipher {
    uint8_t enc_arr[256];
    uint8_t dec_arr[256];
    Cipher(const char *path = NULL) {
        if(!path) return;
		FILE *fp = fopen(path, "rb");
        if (!fp) {
            printf("key file not exit.\n");
            exit(EXIT_FAILURE);
        }
		int i = 0;
        for (i = 0; i < 256; i++) {
            fread(enc_arr + i, 1, sizeof(uint8_t), fp);
        }
        fseek(fp, 256, 0);
        for (i = 0; i < 256; i++) {
            fread(dec_arr + i, 1, sizeof(uint8_t), fp);
        }
        fclose(fp);
    }
    void* encrypt_data(void *buff, size_t nbytes) {
        uint8_t *ptr = (uint8_t *)buff;
        size_t i = 0;
        while (ptr && i < nbytes) {
            *ptr = enc_arr[*ptr];
            ptr++;
            i++;
        }
        return buff;
    }
	void* encrypt_data(std::string &buff) {
		int i;
		for (i = 0; i < buff.size(); i++) {
			buff[i] = enc_arr[(uint8_t)buff[i]];
		}
		return (void *)0;
	}
    void* decrypt_data(void *buff, size_t nbytes) {
        uint8_t *ptr = (uint8_t *)buff;
        size_t i = 0;
        while (ptr && i < nbytes) {
            *ptr = dec_arr[*ptr];
            ptr++;
            i++;
        }
        return buff;
    }
	void* decrypt_data(std::string &buff) {
		int i;
		for (i = 0; i < buff.size(); i++) {
			buff[i] = dec_arr[(uint8_t)buff[i]];
		}
		return (void *)0;
	}
    void gen_key_file(const char *path = "./key.bin") {
        srand((unsigned int)time(NULL));
        int i = 0;
        std::set<uint8_t> __set;
        for (; ;) {
            if(i == 256) break;
            int v = rand() % 256;
			std::set<uint8_t>::iterator it = __set.find(v);
            if(it == __set.end() && v != i) {
                enc_arr[i] = v;
                __set.insert(v);
                i++;
            }
        }
        for (i = 0; i < 256; i++) {
            dec_arr[enc_arr[i]] = i; 
        }
        FILE *wf = fopen(path, "wb+");
        if (!wf) {
            printf("open file failed.\n");
            exit(EXIT_FAILURE);
        }
        for (i = 0; i < 256; i++) {
            fwrite(enc_arr + i, 1, sizeof(uint8_t), wf);
        }
        for (i = 0; i < 256; i++) {
            fwrite(dec_arr + i, 1, sizeof(uint8_t), wf);
        }
        fclose(wf);
    }
};

int recv_data(int fd, IoBuffer &buffer) {
	char buf[MAX_BUF];
	int count = 0;
	while ((count = recv(fd, buf, sizeof(buf), 0)) > 0) {
		buffer.append(buf, count);
	}
	if (buffer.readableBytes() == 0) {
		return -1;
	}
	return 0;

}

int send_data(int fd, const char *buf, int size) {

	int nwrite, data_size = size;
	int n = data_size;
	while (n > 0) {
		nwrite = send(fd, buf + data_size - n, n, 0);
		if (nwrite < n) {
			if (errno == EAGAIN) {
				if (nwrite != -1) {
					n -= nwrite;
				}
			}
			break;
		}
		n -= nwrite;            
	}
	if (n == 0) {
		return 0;
	}
	return -1;
}

int main(int argc, char **argv) {
    
	Cipher *cipher = NULL;
	if (argc == 2 && !strcmp(argv[1], "key")) {
		cipher = new Cipher();
		cipher->gen_key_file();
		exit(0);
	} else if (argc != 4) {
		fprintf(stderr,"Usage: %s <localport> <remotehost> <remoteport>\n",argv[0]);
		return -1;
	}
	cipher = new Cipher("./key.bin");
	int local_sock;
    client_t clients[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].inuse = false;
    }

	WSADATA wsadata;
	WSAStartup(MAKEWORD(1,1), &wsadata);

    sockaddr_in local_addr, remote_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons((unsigned short)atol(argv[1]));

    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(argv[2]);
    remote_addr.sin_port = htons((unsigned short)atol(argv[3]));
    

    if ((local_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("create socket failed!.");
        exit(-1);
    }

	unsigned long int opt = 1;
    if (setsockopt(local_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))) {
        perror("setsockopt reuseaddr failed!.");
        exit(-1);
    }
    if (bind(local_sock, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind error.");
        return -1;
    }
    if (listen(local_sock, 40)) {
        perror("listen error");
    }
	opt = 1;
	if (ioctlsocket(local_sock, FIONBIO, &opt) == -1) {
		printf("ioctlsocket() error %d\n", WSAGetLastError());
        return EXIT_FAILURE;
	}
	
	local_addr.sin_port = htons(0);
    printf("pipe starting\n");
    
	while (true) {
        
        int maxsocks;
        fd_set fd_reads;
        timeval tv = {0, 50000};
        time_t now = time(NULL);

        FD_ZERO(&fd_reads);
        FD_SET(local_sock, &fd_reads);
        maxsocks = local_sock;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].inuse) {
                FD_SET(clients[i].c_sock, &fd_reads);
                if (clients[i].c_sock > maxsocks) {
                    maxsocks = clients[i].c_sock;
                }
                FD_SET(clients[i].r_sock, &fd_reads);
                if (clients[i].r_sock > maxsocks) {
                    maxsocks = clients[i].r_sock;
                }
            }
        }
        if (select(maxsocks + 1, &fd_reads, NULL, NULL, &tv) < 0) {
            return -1;
        }

        if (FD_ISSET(local_sock, &fd_reads)) {
            sockaddr_in addr = {0};
            addr.sin_family = AF_INET;
            int len = sizeof(addr);
            int csock;
            if ((csock = accept(local_sock, (sockaddr*)&addr, &len)) == -1) {
                perror("accept error.");
                exit(-1);
            }
            // printf("client: %s, port: %d connected.\n", inet_ntoa(addr.sin_addr), htons(addr.sin_port));
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if(!clients[i].inuse) {
                    break;
                }
            }
            if (i < MAX_CLIENTS) {
                int osock = socket(AF_INET, SOCK_STREAM, 0);
                // stupid fuck
                if (osock < 0) {
                    perror("socket");
                    closesocket(osock);
                } else if (bind(osock, (sockaddr *)&local_addr, sizeof(local_addr))) {
                    closesocket(osock);
                    closesocket(csock);
                } else if (connect(osock, (sockaddr *)&remote_addr, sizeof(remote_addr))) {
                    perror("connect");
                    closesocket(osock);
                    closesocket(csock);
                } else {
                    // fcntl(clients[i].c_sock, F_SETFL, fcntl(clients[i].c_sock, F_GETFL, 0) | O_NONBLOCK); 
                    // fcntl(clients[i].r_sock, F_SETFL, fcntl(clients[i].r_sock, F_GETFL, 0) | O_NONBLOCK);         
					opt = 1;
					if (ioctlsocket(csock, FIONBIO, &opt) == -1) {
						printf("ioctlsocket() error %d\n", WSAGetLastError());
						return EXIT_FAILURE;
					}
					opt = 1;
					if (ioctlsocket(osock, FIONBIO, &opt) == -1) {
						printf("ioctlsocket() error %d\n", WSAGetLastError());
						return EXIT_FAILURE;
					}
					
					clients[i].c_sock = csock;
                    clients[i].r_sock = osock;
                    clients[i].activity = now;
                    clients[i].inuse = true;
                }
            } else {
                printf("too many clients\n");
                closesocket(csock);
            }
        }
        for (i = 0; i < MAX_CLIENTS; i++) {
            int close_needed = 0;
            if (!clients[i].inuse) {
                continue;
            } else if (FD_ISSET(clients[i].c_sock, &fd_reads)) {
                IoBuffer buffer;
				if (recv_data(clients[i].c_sock, buffer) == 0) {
					std::string temp = buffer.retriveAllString();
					cipher->encrypt_data(temp);
					if (send_data(clients[i].r_sock, temp.c_str(), temp.size()) == 0) {
						clients[i].activity = now;
					}
				} else {
					close_needed = 1;
				}
            } else if (FD_ISSET(clients[i].r_sock, &fd_reads)) {
                IoBuffer buffer;
				if (recv_data(clients[i].r_sock, buffer) == 0) {
					std::string temp = buffer.retriveAllString();
					cipher->decrypt_data(temp);
					if (send_data(clients[i].c_sock, temp.c_str(), temp.size()) == 0) {
						clients[i].activity = now;
					}
				} else {
					close_needed = 1;
				}
            } else if(now - clients[i].activity > IDLETIMEOUT) {
                close_needed = 1;
            }
            if (close_needed) {
                closesocket(clients[i].c_sock);
                closesocket(clients[i].r_sock);
                clients[i].inuse = false;
            }
        }
    }
    delete cipher;
	return 0;
}