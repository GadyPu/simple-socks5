"""
===================================
    -*- coding:utf-8 -*-
    Author     :GadyPu
    E_mail     :Gadypy@gmail.com
    Time       :2022/11/15 0015 下午 12:33
    FileName   :tcpReplay.py
===================================
"""
import logging
import socket
import struct
from event_loop import EventLoop
from cipher import Cipher

BUFFER = 4096
STATUS_INIT = 1
STATUS_CONNECT_REMOTE = 2
STATUS_CONNECTING = 3
SOCKET5_VERSION = 5

class TcpReplay(object):

    def __init__(self, config: dict, sock: socket.socket, loop: EventLoop, fd_to_h: dict, cipher: Cipher):

        self.status = STATUS_INIT
        self.fd_to_handles = fd_to_h
        self.local_sock = sock
        self.config = config
        self.loop = loop
        self.cipher = cipher
        self.remote_sock = None
        self.fd_to_handles[self.local_sock.fileno()] = self
        self.local_sock.setblocking(False)
        self.loop.add(self.local_sock, self)
        self.loop.clear_w_e(self.local_sock.fileno())
        self.is_local = self.config['local']

    def handle_event(self, sock, event):

        if event == 2:
            self.loop.clear_w_e(sock.fileno())
            if not self.is_local:
                replay = struct.pack('!BBBB', SOCKET5_VERSION, 0, 0, 1)
                body = socket.inet_aton(self.config['local_ip']) + struct.pack(">H", self.config['local_port'])
                if not self.write_to_sock(self.cipher.encrypt_data(replay + body), self.local_sock):
                    self.destroy()
            return
        elif event == 3:
            # 链接出错
            self.destroy()
            return
        if sock == self.local_sock:
            self.on_local_read()
            return
        elif sock == self.remote_sock:
            self.on_remote_read()
            return
        else:
            logging.error(f'TcpConnectorLocal接收不到socket:{sock.fileno()}')
            self.destroy()

    def on_local_read(self):
        if not self.local_sock:
            return
        data = None
        try:
            data = self.local_sock.recv(BUFFER)
            if not data:
                self.destroy()
                return
        except socket.error:
            # print('从本地客户端接收数据错误.')
            self.destroy()
            logging.info(f'从本地:{self.local_sock.fileno()}接收数据发生错误.')
        if self.is_local and data:
            if self.status == STATUS_INIT:
                self.handle_connect_remote(data)
            if self.status == STATUS_CONNECTING:
                self.handle_connect_connecting(data)
            else:
                # print('TcpConnectorLocal中的status状态错误')
                logging.error('TcpConnectorLocal中的status状态错误')
        else:
            if not data:
                return
            data = self.cipher.decrypt_data(data)
            if self.status == STATUS_INIT:
                self.handle_connect_init(data)
                return
            if self.status == STATUS_CONNECT_REMOTE:
                self.handle_connect_remote(data)
                return
            if self.status == STATUS_CONNECTING:
                self.handle_connect_connecting(data)
            else:
                # print('TcpConnectorLocal中的status状态错误')
                logging.error('TcpConnectorLocal中的status状态错误')

    def on_remote_read(self):
        if not self.remote_sock:
            print('remote_sock为空')
            return
        data = None
        try:
            data = self.remote_sock.recv(BUFFER)
        except socket.error:
            # print('从远端sock接收数据失败.')
            logging.info(f'从远端:{self.remote_sock.fileno()}接收数据失败.')
        if data:
            if not self.is_local:
                self.write_to_sock(self.cipher.encrypt_data(data), self.local_sock)
            else:
                self.write_to_sock(self.cipher.decrypt_data(data), self.local_sock)
        else:
            self.destroy()

    @staticmethod
    def write_to_sock(data: bytes, sock: socket):
        try:
            bytes_send = 0
            total_bytes = len(data)
            while bytes_send != total_bytes:
                temp = sock.send(data[bytes_send:])
                if temp < 0:
                    return temp
                bytes_send += temp
            return True
        except socket.error as e:
            # print(e)
            return False

    def clear(self, sock: socket):
        fd = sock.fileno()
        if self.fd_to_handles.get(fd):
            del self.fd_to_handles[sock.fileno()]
            self.loop.remove(sock, self)
            sock.close()
            sock = None

    def destroy(self):
        if self.local_sock:
            self.clear(self.local_sock)
        if self.remote_sock:
            self.clear(self.remote_sock)

    def handle_connect_init(self, data):
        if data != b'\x05\x01\x00':
            # print('客户端发送非标准的socks5协议.')
            logging.error('客户端发送非标准的socks5协议.')
            self.destroy()
        if self.write_to_sock(self.cipher.encrypt_data(struct.pack('!BB', SOCKET5_VERSION, 0)), self.local_sock):
            self.status = STATUS_CONNECT_REMOTE

    def create_remote_socket(self, remote_addr, port):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if not self.is_local:
            sock.setblocking(False)
        sock.setsockopt(socket.SOL_TCP, socket.TCP_NODELAY, 1)
        try:
            # connect会抛异常
            if self.is_local:
                sock.connect((remote_addr, port))
            else:
                sock.connect_ex((remote_addr, port))
        except socket.error:
            # print('连接到远端服务器失败')
            logging.error("[-]无法连接的代理服务器.")
        return sock

    def handle_connect_remote(self, data):

        if self.is_local:
            self.remote_sock = self.create_remote_socket(self.config['server_ip'], self.config['server_port'])
            if self.remote_sock:
                self.remote_sock.setblocking(False)
                self.loop.add(self.remote_sock, self)
                self.fd_to_handles[self.remote_sock.fileno()] = self
                self.status = STATUS_CONNECTING
            else:
                self.destroy()
        else:
            if not data:
                self.destroy()
                return
            VER, CMD, _, ATYP = struct.unpack('!BBBB', data[:4])
            if CMD != 1:
                logging.error('this version programming only support "connect mode"')
                self.destroy()
            remote_addr, port = '', 0
            if ATYP == 1:
                recv_ip = data[4: 8]
                remote_addr = socket.inet_ntoa(recv_ip)
                port = struct.unpack('!H', data[8: 10])[0]
            elif ATYP == 3:
                recv_url_len = data[4: 5]
                url_len = struct.unpack('!B', recv_url_len)[0]
                remote_addr = data[5: 5 + url_len]
                port = struct.unpack('!H', data[5 + url_len: 5 + url_len + 2])[0]
            elif ATYP == 4:
                self.destroy()
            # print(ATYP, remote_addr, port)
            self.remote_sock = self.create_remote_socket(remote_addr, port)
            if self.remote_sock:
                self.loop.add(self.remote_sock, self)
                self.fd_to_handles[self.remote_sock.fileno()] = self
                self.status = STATUS_CONNECTING
            else:
                # print('连接到远端地址失败.')
                logging.error(f"无法连接到远端{self.config['server_ip']}.")
                self.destroy()

    def handle_connect_connecting(self, data):
        if data:
            if self.is_local:
                self.write_to_sock(self.cipher.encrypt_data(data), self.remote_sock)
            else:
                self.write_to_sock(data, self.remote_sock)
        else:
            # print('转发至remote_sock失败.')
            logging.error(f'数据转发至远端{self.remote_sock.fileno()}失败.')
            self.destroy()
