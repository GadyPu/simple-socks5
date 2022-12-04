"""
===================================
    -*- coding:utf-8 -*-
    Author     :GadyPu
    E_mail     :Gadypy@gmail.com
    Time       :2022/11/15 0015 下午 12:15
    FileName   :forward.py
===================================
"""
import socket
import select
from concurrent.futures import ThreadPoolExecutor


def process(conn, addr):
    proxy_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    proxy_conn.connect(('192.168.2.113', 9999))
    conn.setblocking(False)
    proxy_conn.setblocking(False)
    closed = False
    while not closed:
        rlist, _, _ = select.select([conn, proxy_conn], [], [])
        for r in rlist:
            w = proxy_conn if r is conn else conn
            try:
                d = r.recv(1024)
                if not d:
                    closed = True
                    break
                # print(d)
                w.sendall(d)
            except:
                closed = True
                break

    try:
        proxy_conn.shutdown(socket.SHUT_RDWR)
        proxy_conn.close()
    except:
        pass

    try:
        conn.shutdown(socket.SHUT_RDWR)
        conn.close()
    except:
        pass


def start():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('0.0.0.0', 1081))
    s.listen(100)
    pool = ThreadPoolExecutor(128)
    print('Server listen on 0.0.0.0:1081')
    while True:
        conn, addr = s.accept()
        # print(conn, addr)
        pool.submit(process, conn, addr)


if __name__ == '__main__':
    start()