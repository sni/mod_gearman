#include <stdio.h>
#include <string.h>
#include "rijndael.h"

#define KEYBITS     256
#define BLOCKSIZE    16

void mod_gm_aes_init(char * password);
int mod_gm_aes_encrypt(unsigned char ** encrypted, char * text);
void mod_gm_aes_decrypt(char ** decrypted, unsigned char * encrypted, int size);
