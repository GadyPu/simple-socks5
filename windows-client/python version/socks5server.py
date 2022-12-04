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
    'server_port': 9999,
    'timeout': '',
    'local': False
}

def work():
    loop = None
    try:
        loop = EventLoop(timeout=CONFIG['timeout'])
        server = Server(CONFIG, './key.bin')
        server.add_to_loop(loop)
        loop.run_forever()
    except Exception as e:
        print(e)
    if loop:
        loop.run_forever()

def usage():
    use = '''A socks5 proxy tool help you pass firewall.
——Catherall People do not lack strength; they lack will.
have fun.
A simple socks5 server v1.0
USAGE:
    sock5local [OPTIONS]
FLAGS:
    -h,--help                 Prints help information
    -v,--version              Prints version information
OPTIONS:
    -k         <key>          set password on server
    -p         <BIND_PORT>    local listen port'''
    print(use)


if __name__ == '__main__':

    os.chdir(os.path.dirname(__file__) or '.')
    if os.path.exists('s_config.json'):
        with open('s_config.json', 'r', encoding='utf-8') as rf:
            CONFIG = json.loads(rf.read())
    try:
        optlist, args = getopt.gnu_getopt(
            sys.argv[1:], 'hvp:', ['help', 'version', 'local_port'])
    except getopt.GetoptError:
        usage()
        sys.exit()
    for key, value in optlist:
        if key == '-p':
            CONFIG['local_port'] = int(value)
        elif key == '-h':
            usage()
            sys.exit()
        elif key == '-v':
            print('socks5server v1.0')
            sys.exit()

    logging.basicConfig(filename='s5server.log', level=logging.DEBUG,
                        format='%(asctime)s [line:%(lineno)d] [%(levelname)s]: %(message)s',
                        filemode='a+')
    if os.path.exists('s_config.json'):
        with open('s_config.json', 'w+', encoding='utf-8') as wf:
            wf.write(json.dumps(CONFIG, ensure_ascii=False))
    work()
