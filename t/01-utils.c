#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <t/tap.h>
#include <worker_logger.h>
#include <common.h>
#include <utils.h>

int main(void) {
    plan_tests(23);

    /* lowercase */
    char test[100];
    ok1(lc(NULL) == NULL);
    strcpy(test, "Yes"); ok1(strcmp(lc(test), "yes") == 0);
    strcpy(test, "YES"); ok1(strcmp(lc(test), "yes") == 0);
    strcpy(test, "yeS"); ok1(strcmp(lc(test), "yes") == 0);

    /* parse_yes_or_no */
    ok1(parse_yes_or_no(NULL,    GM_ENABLED)  == GM_ENABLED);
    ok1(parse_yes_or_no(NULL,    GM_DISABLED) == GM_DISABLED);
    strcpy(test, "");      ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED);
    strcpy(test, "");      ok1(parse_yes_or_no(test, GM_DISABLED) == GM_DISABLED);
    strcpy(test, "yes");   ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED);
    strcpy(test, "true");  ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED);
    strcpy(test, "Yes");   ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED);
    strcpy(test, "1");     ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED);
    strcpy(test, "On");    ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_ENABLED);
    strcpy(test, "Off");   ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED);
    strcpy(test, "false"); ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED);
    strcpy(test, "no");    ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED);
    strcpy(test, "0");     ok1(parse_yes_or_no(test, GM_ENABLED)  == GM_DISABLED);

    /* trim */
    ok1(trim(NULL) == NULL);
    strcpy(test, " test "); ok1(strcmp(trim(test), "test") == 0);

    /* crypt */
    char * key = "test1234";
    char * encrypted = malloc(GM_BUFFERSIZE);
    char * base64    = malloc(GM_BUFFERSIZE);
    char * text = "test message";
    mod_gm_crypt_init(key);
    int len;
    len = mod_gm_encrypt(&encrypted, text, GM_ENCODE_AND_ENCRYPT);
    ok(len == 24, "length of encrypted only");
    ok(!strcmp(encrypted, "RHbm+zWGLlcx+8ItCanpWg=="), "encrypted string");
    len = mod_gm_encrypt(&base64, text, GM_ENCODE_ONLY);
    ok(len == 16, "length of encode only");
    ok(!strcmp(base64, "dGVzdCBtZXNzYWdl"), "base64 only string");

    return exit_status();
}
