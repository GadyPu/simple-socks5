"""
===================================
    -*- coding:utf-8 -*-
    Author     :GadyPu
    E_mail     :Gadypy@gmail.com
    Time       :2022/11/15 0015 下午 12:30
    FileName   :socks5local.py
===================================
"""

from event_loop import EventLoop
from mit_server import Server
import os
import sys
import getopt
import json
import logging

CONFIG = {
    'local_ip': '0.0.0.0',
    'local_port': 1081,
    'server_ip': '192.168.2.113',
    'server_port': 9527,
    'timeout': '',
    'local': True
}


def work():
    loop = None
    try:
        tcp_server = Server(CONFIG, './key.bin')
        loop = EventLoop(timeout=CONFIG['timeout'])
        tcp_server.add_to_loop(loop)
    except Exception as e:
        print(e)
    if loop:
        loop.run_forever()


def usage():
    use = '''A simple port forward tool help you bypass firewall.
——Catherall People do not lack strength; they lack will.
have fun.
client v1.0
USAGE:
    sock5local [OPTIONS]
FLAGS:
    -h,--help                 Prints help information
    -v,--version              Prints version information
OPTIONS:
    -p         <BIND_ADDR>    remote server port
    -s         <BIND_PORT>    remote server ip
    -l         <BIND_PORT>    local listen port'''
    print(use)


if __name__ == '__main__':
    os.chdir(os.path.dirname(__file__) or '.')
    if os.path.exists('l_config.json'):
        with open('l_config.json', 'r', encoding='utf-8') as rf:
            CONFIG = json.loads(rf.read())
    try:
        optlist, args = getopt.gnu_getopt(
            sys.argv[1:], 'hvs:p:l:', ['help', 'version', 'server', 'port', 'local_port'])
    except getopt.GetoptError:
        usage()
        sys.exit()
    for key, value in optlist:
        if key == '-p':
            CONFIG['server_port'] = int(value)
        elif key == '-l':
            CONFIG['local_port'] = int(value)
        elif key == '-s':
            CONFIG['server_ip'] = value
        elif key == '-h':
            usage()
            sys.exit()
        elif key == '-v':
            print('socks5local v1.0')
            sys.exit()

    logging.basicConfig(filename='s5local.log', level=logging.DEBUG,
                        format='%(asctime)s [line:%(lineno)d] [%(levelname)s]: %(message)s',
                        filemode='a+')
    if os.path.exists('l_config.json'):
        with open('l_config.json', 'w+', encoding='utf-8') as wf:
            wf.write(json.dumps(CONFIG, ensure_ascii=False))
    print('A simple tool help you bypass firewall.')
    work()
