#pragma once
#include<string>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<ctime>
#include<set>

typedef unsigned char uint8_t;
struct Cipher {
    uint8_t enc_arr[256];
    uint8_t dec_arr[256];
    Cipher (const char *path = nullptr) {
        if (!path) {
            return;
        }
        FILE *fp = fopen(path, "rb");
        if (!fp) {
            printf("key file not exit.\n");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < 256; i++) {
            fread(enc_arr + i, 1, sizeof(uint8_t), fp);
        }
        fseek(fp, 256, 0);
        for (int i = 0; i < 256; i++) {
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
        for (auto &r :buff) {
            r = enc_arr[(uint8_t)r];
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
    void gen_key_file(const char *path = "./key.bin") {
        srand((unsigned int)time(NULL));
        int i = 0;
        std::set<int> __set;
        for (; ;) {
            if(i == 256) break;
            int v = rand() % 256;
            auto it = __set.find(v);
            if(it == __set.end() && v != i) {
                enc_arr[i] = v;
                __set.insert(v);
                i++;
            }
        }
        for (int i = 0; i < 256; i++) {
            dec_arr[enc_arr[i]] = i; 
        }
        FILE *wf = fopen(path, "wb+");
        if (!wf) {
            printf("open file failed.\n");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < 256; i++) {
            fwrite(enc_arr + i, 1, sizeof(unsigned char), wf );
        }
        for (int i = 0; i < 256; i++) {
            fwrite(dec_arr + i, 1, sizeof(unsigned char), wf );
        }
        fclose(wf);
    }
};