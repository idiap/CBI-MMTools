#!/bin/sh


echo "Bootstrapping autoconf/automake build system for cbi-mm..." 1>&2

autoreconf --force --install --verbose

echo "Bootstrapping complete; now you can run ./configure" 1>&2
echo "If you installed micro-manager in a custom path, run ./configure --prefix=<path/to/micro-manager/install>" 1>&2

