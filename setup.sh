#!/bin/bash

if [[ ! -d "deps" ]]
then
    mkdir deps
fi

# install all dependencies
cd deps

# install openssl
git clone https://github.com/openssl/openssl.git
cd openssl
git reset --hard 31a8925
sudo make install
cd ..

# install zip
git clone https://github.com/kuba--/zip.git
cd zip
git reset --hard 05f412b
cd ..

# install msgpack
git clone https://github.com/msgpack/msgpack-c.git
cd msgpack-c
git reset --hard 6e7deb8
git checkout c_master
cmake .
make
sudo make install

# end
cd ..
