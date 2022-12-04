"""
===================================
    -*- coding:utf-8 -*-
    Author     :GadyPu
    E_mail     :Gadypy@gmail.com
    Time       :2022/11/1 0001 下午 10:37
    FileName   :Server.py
===================================
"""

from __future__ import absolute_import
import socket
from event_loop import EventLoop
from tcpReplay import TcpReplay
from cipher import Cipher
import logging


class Server(object):

    def __init__(self, CONFIG, path):

        self.config = CONFIG
        self.event_loop = None
        self.fd_to_handlers = {}

        self.s_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if self.config['local']:
            self.s_socket.bind(('0.0.0.0', self.config['local_port']))
        else:
            self.s_socket.bind(('0.0.0.0', self.config['server_port']))
        self.s_socket.listen(500)
        self.s_socket.setblocking(False)

        self.cipher = Cipher(path)

    def add_to_loop(self, loop: EventLoop):

        if self.event_loop:
            raise Exception('server already added to loop.')

        self.event_loop = loop
        self.event_loop.add(self.s_socket, self)
        self.event_loop.clear_w_e(self.s_socket.fileno())

        if self.config['local']:
            logging.info(f'[+]proxy listening at {self.config["local_port"]} waiting for connection.')
            print(f'[+]proxy listening at {self.config["local_port"]} waiting for connection.')
        else:
            logging.info(f'[+]proxy listening at {self.config["server_port"]} waiting for connection.')
            print(f'[+]proxy listening at {self.config["server_port"]} waiting for connection.')

    def handle_event(self, sock, event):

        if sock == self.s_socket:
            c_sock, _ = self.s_socket.accept()
            # print(c_sock, _)
            _ = TcpReplay(self.config, c_sock,
                          self.event_loop, self.fd_to_handlers, self.cipher)
