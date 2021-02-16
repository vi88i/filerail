#!/bin/bash

echo "Grab a coffee, sit back and relax till dependencies are downloaded..."

if [[ ! -d "deps" ]]
then
    mkdir deps
fi

# install all dependencies
cd deps

# install openssl
if [[ ! -d "openssl" ]]
then
	git clone https://github.com/openssl/openssl.git
	cd openssl
	git reset --hard 31a8925
	./Configure
	make
	make test	
	cd ..
fi

# install zip
if [[ ! -d "zip" ]]
then
	git clone https://github.com/kuba--/zip.git
	cd zip
	git reset --hard 05f412b
	cd ..
fi

# install msgpack
if [[ ! -d "msgpack-c" ]]
then
	git clone https://github.com/msgpack/msgpack-c.git
	cd msgpack-c
	git reset --hard 6e7deb8
	git checkout c_master
	cmake .
	make
	sudo make install
	cd ..
fi

# end
cd ..
echo "Done"