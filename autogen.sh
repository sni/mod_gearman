#/bin/sh

aclocal && autoheader && automake -a && autoreconf --install --verbose

echo ""
echo " now run ./configure && make && make install"
echo ""
