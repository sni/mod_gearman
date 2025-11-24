#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <t/tap.h>
#include <common.h>
#include <utils.h>
#include <check_utils.h>
#include <gm_crypt.h>
#include "gearman_utils.h"

#include <worker_dummy_functions.c>

mod_gm_opt_t *mod_gm_opt = NULL;
char hostname[GM_SMALLBUFSIZE];
volatile sig_atomic_t shmid;

mod_gm_opt_t * renew_opts(void);
mod_gm_opt_t * renew_opts(void) {
    if(mod_gm_opt != NULL)
        mod_gm_free_opt(mod_gm_opt);

    mod_gm_opt = gm_malloc(sizeof(mod_gm_opt_t));
    int rc = set_default_options(mod_gm_opt);
    ok(rc == 0, "setting default options");

    return mod_gm_opt;
}

int main(void) {
    plan(137);

    /* lowercase */
    char test[100];
    ok(lc(NULL) == NULL, "lc(NULL)");
    strcpy(test, "Yes"); is(lc(test), "yes", "lc(yes)");
    strcpy(test, "YES"); is(lc(test), "yes", "lc(YES)");
    strcpy(test, "yeS"); is(lc(test), "yes", "lc(yeS)");


    /* trim */
    strcpy(test, "    text  "); is(ltrim(test), "text  ",   "ltrim()");
    strcpy(test, "    text  "); is(rtrim(test), "    text", "rtrim()");
    strcpy(test, "    text  "); is(trim(test),  "text",     "trim()");
    char *test2;
    test2 = strdup("   text   ");  is(trim(test2),  "text", "trim()");
    free(test2);

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
    strcpy(test, " test "); is(trim(test), "test", "trim(' test ')");
    strcpy(test, "\ntest\n"); is(trim(test), "test", "trim('\\ntest\\n')");

    /* reading keys */
    renew_opts();

    mod_gm_opt->keyfile = strdup("t/data/test1.key");
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
    is(test, "3131313131313131313131313131313131313131313131313131313131310000", "read keyfile t/data/test1.key");

    free(mod_gm_opt->keyfile);
    mod_gm_opt->keyfile = strdup("t/data/test2.key");
    read_keyfile(mod_gm_opt);

    is(mod_gm_opt->crypt_key, "abcdef", "reading keyfile t/data/test2.key");

    free(mod_gm_opt->keyfile);
    mod_gm_opt->keyfile = strdup("t/data/test3.key");
    read_keyfile(mod_gm_opt);
    //printf_hex(mod_gm_opt->crypt_key, 32);
    is(mod_gm_opt->crypt_key, "11111111111111111111111111111111", "reading keyfile t/data/test3.key");
    ok(strlen(mod_gm_opt->crypt_key) == 32, "key size for t/data/test3.key");

    // decode base 64 containing newlines
    {
        char * debase64 = NULL;
        char * plaintext = "test test test test test test test testtest test test testtest test test testtest test test test test test test testtest test test testtest test test test\n";
        char * base64_with_newlines = ""
            "dGVzdCB0ZXN0IHRlc3QgdGVzdCB0ZXN0IHRlc3QgdGVzdCB0ZXN0dGVzdCB0ZXN0IHRlc3QgdGVz\n"
            "dHRlc3QgdGVzdCB0ZXN0IHRlc3R0ZXN0IHRlc3QgdGVzdCB0ZXN0IHRlc3QgdGVzdCB0ZXN0IHRl\n"
            "c3R0ZXN0IHRlc3QgdGVzdCB0ZXN0dGVzdCB0ZXN0IHRlc3QgdGVzdAo=";
        int rc = mod_gm_decrypt(NULL, &debase64, base64_with_newlines, strlen(base64_with_newlines), GM_ENCODE_ONLY);
        cmp_ok(rc, ">", 0, "decrypt worked", rc);
        is(debase64, plaintext, "decoded base64 text is equal to source text");
        free(debase64);
    };

    /* base 64 en/decoding */
    struct {
            const char *plaintext;
            const char *base64;
            int base64_len;
    } base64_tests[] = {
            { "test", "dGVzdA==", 8 },
            { "test\n", "dGVzdAo=", 8 },
            { "test message", "dGVzdCBtZXNzYWdl", 16 },
            { "test1 message\ntest2 message\ntest3 message1234567\n", "dGVzdDEgbWVzc2FnZQp0ZXN0MiBtZXNzYWdlCnRlc3QzIG1lc3NhZ2UxMjM0NTY3Cg==", 68 },
            { NULL, NULL, 0 },
    };
    for (i = 0; base64_tests[i].plaintext != NULL; i++) {
        char * base64 = NULL;
        /* encode base64 */
        int len = mod_gm_encrypt(NULL, &base64, base64_tests[i].plaintext, GM_ENCODE_ONLY);
        cmp_ok(len, "==", base64_tests[i].base64_len, "length of base64 string");
        is(base64, base64_tests[i].base64, "base64 encoded string");

        /* decode base64 */
        char * debase64 = NULL;
        int rc = mod_gm_decrypt(NULL, &debase64, base64, strlen(base64), GM_ENCODE_ONLY);
        cmp_ok(rc, ">", 0, "decrypt worked", rc);
        is(debase64, base64_tests[i].plaintext, "decoded base64 text is equal to source text");
        free(debase64);
        free(base64);
    }

    /* aes en/decryption */
    struct {
            const char *plaintext;
            const char *base64;
            int base64_len;
    } encryption_tests[] = {
            { "test message", "a7HqhQEE8TQBde9uknpPYQ==", 24 },
            { "test1 message\ntest2 message\ntest3 message1234567\n", "lixUQN83MnLhMB6ppyNA5bUPq39eZE+8GnSWLu4JdKJN2uIjOtjjtVn8mZrXj0dLl7iWqId8FZE2j6Ej+jroEQ==", 88 },
            { "123456789abcde", "U+0gPoTCEibvwB+HaO3seA==", 24 },
            { "123456789abcdef", "uLgRxE2qLExwLvEyB7yAEw==", 24 },
            { "123456789abcdef1", "CueC0iJAZL2J+zhEPgVFVYDKd5Fmk6oJnfJnxuj7f0U=", 44 },
            { "123456789abcdef12", "CueC0iJAZL2J+zhEPgVFVeiqJ0EDJEmrqx95Bewle4s=", 44 },
            { "123456789abcdef123", "CueC0iJAZL2J+zhEPgVFVUPX+fNRaJ7/VNIrGRAapGQ=", 44 },
            { NULL, NULL, 0 },
    };
    const char *key = "test1234";
    EVP_CIPHER_CTX * ctx = mod_gm_crypt_init(key);
    for (i = 0; encryption_tests[i].plaintext != NULL; i++) {
        /* encrypt */
        char * encrypted;
        int len = mod_gm_encrypt(ctx, &encrypted, encryption_tests[i].plaintext, GM_ENCODE_AND_ENCRYPT);
        cmp_ok(len, "==", encryption_tests[i].base64_len, "length of encrypted text: %d vs. %d", len, encryption_tests[i].base64_len);
        is(encrypted, encryption_tests[i].base64, "encrypted string");

        /* decrypt */
        char * decrypted = NULL;
        int rc = mod_gm_decrypt(ctx, &decrypted, encrypted, strlen(encrypted), GM_ENCODE_AND_ENCRYPT);
        cmp_ok(rc, "==", rc, "decrypt worked", rc);
        is(decrypted, encryption_tests[i].plaintext, "decrypted text");
        cmp_ok(strlen(encryption_tests[i].plaintext), "==", strlen(decrypted), "decryption string len");
        free(decrypted);
        free(encrypted);
    }

    char *base64only = "dHlwZT1hY3RpdmUKaG9zdF9uYW1lPWhvc3RuYW1lMTIzCmNvcmVfc3RhcnRfdGltZT0xNjc1MzcyODM0LjAwMDAwOApzdGFydF90aW1lPTE2NzUzNzI4MzQuMDAwMDAwCmZpbmlzaF90aW1lPTE2NzUzNzI4MzQuMDAwMDAwCnJldHVybl9jb2RlPTAKZXhpdGVkX29rPTEKc291cmNlPU1vZC1HZWFybWFuIFdvcmtlciBAIGhvc3RuYW1lMTIzCm91dHB1dD1PSyAtIGhvc3RuYW1lMTIzOiBydGEgMjguODk1bXMsIGxvc3QgMCV8cnRhPTI4Ljg5NW1zOzUwMDAuMDAwOzUwMDAuMDAwOzA7IHBsPTAlOzEwMDsxMDA7OyBydG1heD0yOS4wNTdtczs7OzsgcnRtaW49MjguNjkxbXM7Ozs7CgoK";
    char * decrypted = NULL;
    int rc = mod_gm_decrypt(ctx, &decrypted, base64only, strlen(base64only), GM_ENCODE_ACCEPT_ALL);
    cmp_ok(rc, "==", rc, "decrypt worked", rc);
    like(decrypted, "type=active", "plain base64 contains string I");
    like(decrypted, "source=Mod-Gearman", "plain base64 contains string II");
    like(decrypted, "output=OK - hostname123", "plain base64 contains string II");
    free(decrypted);

    mod_gm_crypt_deinit(ctx);

    /* file_exists */
    ok(file_exists("01_utils") == 1, "file_exists('01_utils')");
    ok(file_exists("non-exist") == 0, "file_exists('non-exist')");

    /* nr2signal */
    char * signame1 = nr2signal(9);
    is(signame1, "SIGKILL", "get SIGKILL for 9");
    free(signame1);

    char * signame2 = nr2signal(15);
    is(signame2, "SIGTERM", "get SIGTERM for 15");
    free(signame2);


    /* string2timeval */
    struct timeval t;
    string2timeval("100.000050", &t);
    ok(t.tv_sec  == 100, "string2timeval 1");
    ok(t.tv_usec == 50, "string2timeval 2");
    ok(fabs((double)timeval2double(&t) - 100.00005) < 0.00001, "timeval2double 1");

    string2timeval("100.5", &t);
    ok(t.tv_sec  == 100, "string2timeval 1b");
    ok(t.tv_usec == 500000, "string2timeval 2b");
    ok(fabs((double)timeval2double(&t) - 100.5) < 0.00001, "timeval2double 2");

    string2timeval("100", &t);
    ok(t.tv_sec  == 100, "string2timeval 3");
    ok(t.tv_usec == 0, "string2timeval 4");

    string2timeval("", &t);
    ok(t.tv_sec  == 0, "string2timeval 5");
    ok(t.tv_usec == 0, "string2timeval 6");

    string2timeval(NULL, &t);
    ok(t.tv_sec  == 0, "string2timeval 7");
    ok(t.tv_usec == 0, "string2timeval 8");

    /* command line parsing */
    renew_opts();
    strcpy(test, "server=host:4730");
    parse_args_line(mod_gm_opt, test, 0);
    is(mod_gm_opt->server_list[0]->host, "host", "server=host:4730");
    ok(mod_gm_opt->server_list[0]->port == 4730, "server=host:4730");
    ok(mod_gm_opt->server_num == 1, "server_number = %d", mod_gm_opt->server_num);

    renew_opts();
    strcpy(test, "server=:4730");
    parse_args_line(mod_gm_opt, test, 0);
    is(mod_gm_opt->server_list[0]->host, "0.0.0.0", "server=:4730");
    ok(mod_gm_opt->server_list[0]->port == 4730, "server=:4730");
    ok(mod_gm_opt->server_num == 1, "server_number = %d", mod_gm_opt->server_num);

    renew_opts();
    strcpy(test, "server=localhost:4730");
    parse_args_line(mod_gm_opt, test, 0);
    strcpy(test, "server=localhost:4730");
    parse_args_line(mod_gm_opt, test, 0);
    is(mod_gm_opt->server_list[0]->host, "localhost", "duplicate server");
    ok(mod_gm_opt->server_list[0]->port == 4730, "duplicate server");
    ok(mod_gm_opt->server_num == 1, "server_number = %d", mod_gm_opt->server_num);

    renew_opts();
    strcpy(test, "server=localhost:4730,localhost:4730,:4730,host:4730,");
    parse_args_line(mod_gm_opt, test, 0);
    is(mod_gm_opt->server_list[0]->host, "localhost", "duplicate server");
    ok(mod_gm_opt->server_list[0]->port == 4730, "duplicate server");
    is(mod_gm_opt->server_list[2]->host, "host", "duplicate server");
    ok(mod_gm_opt->server_list[2]->port == 4730, "duplicate server");
    ok(mod_gm_opt->server_num == 3, "server_number = %d", mod_gm_opt->server_num);

    /* escape newlines */
    char * escaped = gm_escape_newlines(" test\n", GM_DISABLED);
    is(escaped, " test\\n", "untrimmed escape string");
    free(escaped);
    escaped = gm_escape_newlines(" test\n", GM_ENABLED);
    is(escaped, "test", "trimmed escape string");
    free(escaped);

    /* sha256 hash sum */
    char * sum;
    strcpy(test, "");
    sum = (char*)mod_gm_hexsum(test);
    is(sum, "3490E034A1F8B9F65333848E51F32C519FD49E727910A0E998968AD8C2C87EC3", "sha256sum()");
    free(sum);

    strcpy(test, "The quick brown fox jumps over the lazy dog.");
    sum = (char*)mod_gm_hexsum(test);
    is(sum, "5C8C8F7E5314C1A46211ABCBC5024700CFFFC8EC719F11A5369683B11CACD72F", "sha256sum()");
    free(sum);

    /* starts_with */
    strcpy(test, "test123");
    test2 = strdup("test");
    ok(starts_with(test2, test) == TRUE, "starts_with(test, test123)");
    free(test2);
    test2 = strdup("test123");
    ok(starts_with(test2, test) == TRUE,  "starts_with(test123, test123)");
    free(test2);
    test2 = strdup("test1234");
    ok(starts_with(test2, test) == FALSE,  "starts_with(test1234, test123)");
    free(test2);
    test2 = strdup("xyz");
    ok(starts_with(test2, test) == FALSE,  "starts_with(xyz, test123)");
    free(test2);

    char uniq[GM_SMALLBUFSIZE];
    make_uniq(uniq, "%s", "test - test");
    is(uniq, "31121DCCD068B90014ADCC5D500F0E0F5C49C8CAE5E893C58FE3C77283B83086", "make_uniq()");
    ok(strlen(uniq) == 64, "length of uniq string is 64");
    ok(strlen(uniq) <= GEARMAN_MAX_UNIQUE_SIZE, "uniq string is smaller than GEARMAN_MAX_UNIQUE_SIZE");

    make_uniq(uniq, "%s", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    is(uniq, "6915CE3CDB89789E8B6DD53FC103E7E7ADD6873AD41A0F40274E5CB63D6B0D04", "make_uniq()");
    ok(strlen(uniq) == 64, "length of uniq string is 64");
    ok(strlen(uniq) <= GEARMAN_MAX_UNIQUE_SIZE, "uniq string is smaller than GEARMAN_MAX_UNIQUE_SIZE");

    make_uniq(uniq, "%s-%s", "xxx-xxxxx-xxxxxx.xxxxxxxxxx.xxxxxx.xx", "xxxx_xxxx_xxxxx_xxx_xx");
    is(uniq, "9E94C0A8F100FCB3EC917C14D0B766FBFF06F40868DD6E6345AA19EB692E9224", "make_uniq()");
    ok(strlen(uniq) == 64, "length of uniq string is 64");
    ok(strlen(uniq) <= GEARMAN_MAX_UNIQUE_SIZE, "uniq string is smaller than GEARMAN_MAX_UNIQUE_SIZE");

    mod_gm_free_opt(mod_gm_opt);

    return exit_status();
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
