#/bin/sh

aclocal && \
    autoheader && \
    automake -a && \
    autoreconf -if

echo ""
echo " now run ./configure && make && make install"
echo ""
