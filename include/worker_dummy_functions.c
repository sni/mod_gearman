char * nebtype2str(__attribute__((__unused__)) int i) {
    return strdup("UNKNOWN");
}

char * nebcallback2str(__attribute__((__unused__)) int i) {
    return strdup("UNKNOWN");
}

char * eventtype2str(__attribute__((__unused__)) int i) {
    return strdup("UNKNOWN");
}

void nm_log(__attribute__((__unused__)) int, __attribute__((__unused__))const char *, ...);
void nm_log(__attribute__((__unused__)) int, __attribute__((__unused__))const char *, ...) {
    return;
}
