#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <crypt.h>

int encryption_initialized = 0;
unsigned char key[KEYLENGTH(KEYBITS)];


/* initialize encryption */
void mod_gm_blowfish_init(char * password) {

    /* pad key till keysize */
    int i;
    for (i = 0; i < 32; i++)
        key[i] = *password != 0 ? *password++ : 0;

    encryption_initialized = 1;
}


/* encrypt text with given key */
int mod_gm_blowfish_encrypt(unsigned char ** encrypted, char * text) {

    assert(encryption_initialized == 1);

    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds;
    nrounds = rijndaelSetupEncrypt(rk, key, 256);
    int size = strlen(text);
    int totalsize = size + 16-size%16;
    int i = 0;
    int k = 0;
    unsigned char *enc;
    enc = (unsigned char *) malloc(sizeof(unsigned char)*totalsize);
    while(size > 0) {
        unsigned char plaintext[16];
        unsigned char ciphertext[16];
        int j;
        for (j = 0; j < 16; j++) {
            int c = text[i];
            if(c == 0)
                break;
            plaintext[j] = c;
            i++;
        }

        for (; j < 16; j++)
            plaintext[j] = ' ';
        rijndaelEncrypt(rk, nrounds, plaintext, ciphertext);
        for (j = 0; j < 16; j++)
            enc[k++] = ciphertext[j];
        size -=16;
    }

    *encrypted = enc;
    return totalsize;
}


/* decrypt text with given key */
void mod_gm_blowfish_decrypt(char ** text, unsigned char * encrypted, int size) {

    assert(encryption_initialized == 1);

    char decr[8192];
    decr[0] = '\0';
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds;
    nrounds = rijndaelSetupDecrypt(rk, key, 256);
    int i = 0;
    while(1) {
        unsigned char plaintext[16];
        unsigned char ciphertext[16];
        int j;
        for (j = 0; j < 16; j++) {
            int c = encrypted[i];
            ciphertext[j] = c;
            i++;
        }
        rijndaelDecrypt(rk, nrounds, ciphertext, plaintext);
        strncat(decr, (char*)plaintext, 16);
        size -= 16;
        if(size < 16)
            break;
    }

    strcpy(*text, decr);
    return;
}
