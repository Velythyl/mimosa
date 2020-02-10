#!/bin/bash
# This script will prepare the environment so Gambit can be built on
# the ubuntu-6 VM which represents the Mimosa target.
set -e

NPROC=$( nproc )
if [ $NPROC -gt 2 ]; then
    NPROC=$(( NPROC - 2 ))
fi

build_gambit() {
    git clone https://github.com/SamuelYvon/gambit
    cd gambit
    GAMBIT_VERSION=$( git tag | grep -v bootstrap | tail -1 | sed 's/\./_/g')
    
    # Just in case
    ./configure
    make -j $NPROC
    make bootstrap
    make bootclean
    make -j $NPROC
    make dist

    mv "gambit-$GAMBIT_VERSION.tgz" ../libc
    cd -

    rm -rf gambit
}

if [ "x$1" != x ]; then
    echo "Building Gambit"
    build_gambit
fi


echo "Copying to VM..."

tar --exclude-vcs --exclude-vcs-ignore --exclude='*.img' -czf - ./libc | ssh administrator@localhost -p 10022 "rm -rf ./libc; tar xzf -;";

echo "Done!"
