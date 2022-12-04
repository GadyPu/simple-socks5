"""
===================================
    -*- coding:utf-8 -*-
    Author     :GadyPu
    E_mail     :Gadypy@gmail.com
    Time       :2022/11/1 0001 下午 09:48
    FileName   :event_loop.py
===================================
"""
import select
import socket
import time


class SelectLoop(object):

    def __init__(self):
        self.r_lists = set()  # 可读文件描述符集合
        self.w_lists = set()  # 可写文件描述符集合
        self.e_lists = set()  # 错误文件描述符集合

    def poll(self, timeout=None):

        reads, writes, errors = select.select(self.r_lists, self.w_lists, self.e_lists, timeout)
        temp = []
        for temp_r in reads:
            t = (temp_r, 1)
            temp.append(t)
        for temp_w in writes:
            t = (temp_w, 2)
            temp.append(t)
        for temp_e in errors:
            t = (temp_e, 3)
            temp.append(t)
        return temp

    def register(self, fd):
        # 注册文件描述符
        # set只插入1次
        if self.r_lists.__contains__(fd):
            print(f'select中的r_lists已经包含{fd}.')
        self.r_lists.add(fd)
        if self.w_lists.__contains__(fd):
            print(f'select中的w_lists已经包含{fd}.')
        self.w_lists.add(fd)
        if self.e_lists.__contains__(fd):
            print(f'select中的e_lists已经包含{fd}.')
        self.e_lists.add(fd)

    def unregister(self, fd):
        # 输出文件描述符
        if self.r_lists.__contains__(fd):
            self.r_lists.remove(fd)
        if self.w_lists.__contains__(fd):
            self.w_lists.remove(fd)
        if self.e_lists.__contains__(fd):
            self.e_lists.remove(fd)

    def clear_w_e_fd(self, fd):
        # 清除可写，错误集合中的文件描述符
        if self.w_lists.__contains__(fd):
            self.w_lists.remove(fd)
        if self.e_lists.__contains__(fd):
            self.e_lists.remove(fd)


class EventLoop(object):

    def __init__(self, timeout):
        self.is_dead = False

        if hasattr(select, 'select'):
            self._loop = SelectLoop()
        else:
            raise Exception("This os don't have select model.")
        self.fd_map = {}  # {fd: (socket, handler)}
        self.TIMEOUT = None if timeout == '' else int(timeout)

    def poll(self, timeout):
        events = self._loop.poll(timeout)
        result = []
        for event in events:
            # socket的fd socket 事件类型(1.可读 2.可写 3.错误)
            t = (event[0], self.fd_map[event[0]][0], event[1])
            result.append(t)
        return result

    def close(self):
        self.is_dead = True

    def add(self, sock: socket.socket, handler):

        fd = sock.fileno()
        if self.fd_map.__contains__(fd):
            print(f'fd_map中已存在{fd}.')
        self.fd_map[fd] = (sock, handler)
        self._loop.register(fd)

    def remove(self, sock: socket.socket, handler):

        if sock:
            fd = sock.fileno()
        if not self.fd_map.get(fd):
            print(f'fd_map中不存在{fd}.')

        else:
            del self.fd_map[fd]
        self._loop.unregister(fd)

    def clear_w_e(self, fd):

        self._loop.clear_w_e_fd(fd)

    def run_forever(self):
        events = []

        while not self.is_dead:

            try:
                events = self.poll(self.TIMEOUT)
            except Exception as e:
                print(e)

            for fd, sock, event in events:

                try:
                    if sock.fileno() < 0:
                        # print('文件描述符小于零.')
                        break
                    # 如果sock存在则取出对应的处理方法
                    handler = None
                    if self.fd_map[sock.fileno()][0]:
                        handler = self.fd_map[sock.fileno()][1]
                    else:
                        self.remove(sock.fileno())
                        print('未找到相应的socket')

                    handler.handle_event(sock, event)
                except (OSError, IOError) as e:
                    print(e)
            time.sleep(0.01)
