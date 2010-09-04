#include <stdio.h>
#include <string.h>
#include "rijndael.h"

#define KEYBITS 256

void mod_gm_blowfish_init(char * password);
int mod_gm_blowfish_encrypt(unsigned char ** encrypted, char * text);
void mod_gm_blowfish_decrypt(char ** decrypted, unsigned char * encrypted, int size);
