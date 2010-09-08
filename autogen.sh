#/bin/sh

aclocal && autoheader && automake -a && autoreconf --install --verbose

echo " now run ./configure && make && make install"
