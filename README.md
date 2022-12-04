# tinysocks5
A simple socks5 proxy tool written in c/c++, supports non-authenticated connection mode, and supports simple data encryption.
just a toy, only for learning linux socket programming.

## environment
- server:linux Ubuntu 18.04.6 LTS
- client:windows

## usage
### 1. complie
- #### server g++
```bash
git clone simple-socks5
cd tinysocks5
make
```
- #### clent vc++6.0

### 2. command

**client**

```bash
Usage: pipe.exe <localport> <remotehost> <remoteport>
```
**server**
```bash
./tinysocks5 
A simple socks5 proxy tool help you bypass firewall
awesome c/c++ linux
author: funy boy ^_^
usage: ./tinysocks5 [options]
options: 
  -a <address>         Local Address to bind (default: 0.0.0.0).
  -p <port>            Port number to bind (default: 9527).
  -k [optional]        use simple algorithm to encrypt transform data.
  -g [optional]        generate key file.
  -o [optional]        turn off EPOLLET mod.
  -v                   show this programm version.
  -h                   Show this help message.

```
## version
### v1.0
The first version only supports the sock5 protocol without a password, and only supports TCP proxy.

### protocol
*MIT*
