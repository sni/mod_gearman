#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <t/tap.h>
#include <worker_logger.h>
#include <common.h>
#include <utils.h>


void printf_hex(char*, int);
void printf_hex(char* text, int length) {
    int i;
    for(i=0; i<length; i++)
        printf("%02x",text[i]);
    printf("\n");
    return;
}

int main(void) {
    plan(42);

    /* lowercase */
    char test[100];
    ok(lc(NULL) == NULL, "lc(NULL)");
    strcpy(test, "Yes"); ok(strcmp(lc(test), "yes") == 0, "lc(yes)");
    strcpy(test, "YES"); ok(strcmp(lc(test), "yes") == 0, "lc(YES)");
    strcpy(test, "yeS"); ok(strcmp(lc(test), "yes") == 0, "lc(yeS)");


    /* parse_yes_or_no */
    ok(parse_yes_or_no(NULL,    GM_ENABLED)  == GM_ENABLED, "parse_yes_or_no 1");
    ok(parse_yes_or_no(NULL,    GM_DISABLED) == GM_DISABLED, "parse_yes_or_no 2");
    strcpy(test, "");      ok(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED, "parse_yes_or_no 3");
    strcpy(test, "");      ok(parse_yes_or_no(test, GM_DISABLED) == GM_DISABLED, "parse_yes_or_no 4");
    strcpy(test, "yes");   ok(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED, "parse_yes_or_no 5");
    strcpy(test, "true");  ok(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED, "parse_yes_or_no 6");
    strcpy(test, "Yes");   ok(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED, "parse_yes_or_no 7");
    strcpy(test, "1");     ok(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED, "parse_yes_or_no 8");
    strcpy(test, "On");    ok(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED, "parse_yes_or_no 9");
    strcpy(test, "Off");   ok(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED, "parse_yes_or_no 10");
    strcpy(test, "false"); ok(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED, "parse_yes_or_no 11");
    strcpy(test, "no");    ok(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED, "parse_yes_or_no 12");
    strcpy(test, "0");     ok(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED, "parse_yes_or_no 13");


    /* trim */
    ok(trim(NULL) == NULL, "trim(NULL)");
    strcpy(test, " test "); ok(strcmp(trim(test), "test") == 0, "trim(' test ')");

    /* reading keys */
    mod_gm_opt_t *mod_gm_opt;
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    int rc = set_default_options(mod_gm_opt);
    ok(rc == 0, "setting default options");
    mod_gm_opt->keyfile = "t/data/test1.key";
    read_keyfile(mod_gm_opt);
    //printf_hex(mod_gm_opt->crypt_key, 32);
    test[0]='\x0';
    int i = 0;
    char hex[4];
    for(i=0; i<32; i++) {
        hex[0] = '\x0';
        snprintf(hex, 4, "%02x", mod_gm_opt->crypt_key[i]);
        strncat(test, hex, 4);
    }
    if(!ok(strcmp(test, "3131313131313131313131313131313131313131313131313131313131310000") == 0, "read keyfile t/data/test1.key"))
        diag("expected: '3131313131313131313131313131313131313131313131313131313131310000'\n but got: '%s'", test );

    mod_gm_opt->keyfile = "t/data/test2.key";
    read_keyfile(mod_gm_opt);
    //printf_hex(mod_gm_opt->crypt_key, 32);
    if(!ok(strcmp(mod_gm_opt->crypt_key, "abcdef") == 0, "reading keyfile t/data/test2.key"))
        diag("expected: 'abcdef'\n but got: '%s'", mod_gm_opt->crypt_key );

    mod_gm_opt->keyfile = "t/data/test3.key";
    read_keyfile(mod_gm_opt);
    //printf_hex(mod_gm_opt->crypt_key, 32);
    if(!ok(strcmp(mod_gm_opt->crypt_key, "11111111111111111111111111111111") == 0, "reading keyfile t/data/test3.key"))
        diag("expected: '11111111111111111111111111111111'\n but got: '%s'", mod_gm_opt->crypt_key );
    ok(strlen(mod_gm_opt->crypt_key) == 32, "key size for t/data/test3.key");


    /* encrypt */
    char * key       = "test1234";
    char * encrypted = malloc(GM_BUFFERSIZE);
    char * text      = "test message";
    char * base      = "a7HqhQEE8TQBde9uknpPYQ==";
    mod_gm_crypt_init(key);
    int len;
    len = mod_gm_encrypt(&encrypted, text, GM_ENCODE_AND_ENCRYPT);
    ok(len == 24, "length of encrypted only");
    if(!ok(!strcmp(encrypted, base), "encrypted string"))
        diag("expected: '%s' but got: '%s'", base, encrypted);

    /* decrypt */
    char * decrypted = malloc(GM_BUFFERSIZE);
    mod_gm_decrypt(&decrypted, encrypted, GM_ENCODE_AND_ENCRYPT);
    if(!ok(!strcmp(decrypted, text), "decrypted text"))
        diag("expected: '%s' but got: '%s'", text, decrypted);
    free(decrypted);
    free(encrypted);

    /* base 64 */
    char * base64 = malloc(GM_BUFFERSIZE);
    len = mod_gm_encrypt(&base64, text, GM_ENCODE_ONLY);
    ok(len == 16, "length of encode only");
    ok(!strcmp(base64, "dGVzdCBtZXNzYWdl"), "base64 only string");

    /* debase 64 */
    char * debase64 = malloc(GM_BUFFERSIZE);
    mod_gm_decrypt(&debase64, base64, GM_ENCODE_ONLY);
    if(!ok(!strcmp(debase64, text), "debase64 text"))
        diag("expected: '%s' but got: '%s'", text, debase64);
    free(debase64);
    free(base64);


    /* file_exists */
    ok(file_exists("01_utils") == 1, "file_exists('01_utils')");
    ok(file_exists("non-exist") == 0, "file_exists('non-exist')");

    /* nr2signal */
    char * signame1 = nr2signal(9);
    if(!ok(!strcmp(signame1, "SIGKILL"), "get SIGKILL for 9"))
        diag("expected: 'SIGKILL' but got: '%s'", signame1);
    free(signame1);

    char * signame2 = nr2signal(15);
    if(!ok(!strcmp(signame2, "SIGTERM"), "get SIGTERM for 15"))
        diag("expected: 'SIGTERM' but got: '%s'", signame2);
    free(signame2);


    /* string2timeval */
    struct timeval t;
    string2timeval("100.50", &t);
    ok(t.tv_sec  == 100, "string2timeval 1");
    ok(t.tv_usec == 50, "string2timeval 2");

    string2timeval("100", &t);
    ok(t.tv_sec  == 100, "string2timeval 3");
    ok(t.tv_usec == 0, "string2timeval 4");

    string2timeval("", &t);
    ok(t.tv_sec  == 0, "string2timeval 5");
    ok(t.tv_usec == 0, "string2timeval 6");

    string2timeval(NULL, &t);
    ok(t.tv_sec  == 0, "string2timeval 7");
    ok(t.tv_usec == 0, "string2timeval 8");

    return exit_status();
}
