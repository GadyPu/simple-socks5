"""
===================================
    -*- coding:utf-8 -*-
    Author     :GadyPu
    E_mail     :Gadypy@gmail.com
    Time       :2022/11/3 0003 下午 07:12
    FileName   :cipher.py
===================================
"""
class Cipher(object):

    def __init__(self, path):
        with open(path, 'rb') as rf:
            self.enc_arr = rf.read(256)
            self.dec_arr = rf.read(256)

    def encrypt_data(self, data):
        ret = bytearray(len(data))
        for i, j in enumerate(data):
            ret[i] = self.enc_arr[j]
        return ret

    def decrypt_data(self, data):
        ret = bytearray(len(data))
        for i, j in enumerate(data):
            ret[i] = self.dec_arr[j]
        return ret

if __name__ == '__main__':

    cipher = Cipher("./key.bin")
    s1 = '我是xxca'.encode()
    xx = cipher.encrypt_data(s1)
    print(xx, cipher.decrypt_data(xx).decode())