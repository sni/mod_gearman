#/bin/sh

autoreconf -if
./configure --enable-silent-rules "$@"
