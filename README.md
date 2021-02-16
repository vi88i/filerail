<p align="center"><img src="https://github.com/vi88i/filerail/blob/main/assets/filerail.png" alt="filerail"></p>

<p align="center"><b>Host a simple and cheap peer-to-peer file transfer program on your Linux server.</b></p>

---

# Features

- Single command upload and download feature.
- Checkpointing download and upload, and resume back whenever you are back online.
- Compresses your data before sending.
- Encryption using AES-128 in CBC mode of operation.
- Uses MD5 hash to verify integrity at receiver side.
- Uses <a href="https://msgpack.org/index.html">MessagePack</a> for data interchange, to increase portablility among linux different systems.

# How filerail works ?

An application layer protocol built on top of TCP. After connection establishment, client and server exchange messages to check if request made by client is feasible. Once request is deemed feasible, file/directory transfer process starts.

---

# Install

```bash
$ git clone https://github.com/vi88i/filerail.git
$ cd filerail
$ chmod +x setup.sh
$ ./setup.sh
```

---

# Setup

## On server side

- Spin filerail server.

```bash
$ gcc -I./deps/zip/src -o filerail_server filerail_server.c ./deps/msgpack-c/libmsgpackc.a ./deps/openssl/libcrypto.a
```

```bash
# usage: -v -d [-i ipv4 address] [-p port] [-k key path] [-c checkpoints directory]
```

```bash
$ ./filerail_server -v -i 127.0.0.1 -p 8000 -k /home/key -c /home/server_ckpts
```

```text
options:
1. -u : usage
2. -v : verbose mode on
3. -d : DNS resolution of provided DNS name
4. -i : IPv4 address of server
5. -p : port
6. -k : key path (requires absolute path to key file)
7. -c : checkpoints directory (requires absolute path to checkpoints directory)
```

- To check if server is running

```bash
$ sudo netstat -pln | grep 8000
# if it doen't display anything something went wrong, refer syslog
$ cat /var/log/syslog | grep filerail
```

## On client side

- Compile client side code.

```bash
$ gcc -I./deps/zip/src -o filerail_client filerail_client.c ./deps/msgpack-c/libmsgpackc.a ./deps/openssl/libcrypto.a
```

```bash
# usage: -v -d [-i ipv4 address] [-p port] [-o operation] [-r resource path] 
#           [-d destination path] [-k key path] [-c checkpoints directory]
```

```text
options:
1. -u : usage
2. -v : verbose mode on
3. -d : DNS resolution of provided DNS name
4. -i : IPv4 address of server
5. -p : port
6. -o : operation {put, get, ping}
7. -r : resource path (requires absolute path to resource)
8. -d : destination path (requires absolute path to destination)
9. -k : key path (requires absolute path to key file)
10. -c : checkpoints directory (requires absolute path to checkpoints directory)
```

### ping

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o ping -k /home/key -c /home/ckpt
PONG
```

### Upload file/directory

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o put -r /home/user/a -d /home/user/fun -k /home/key -c /home/ckpt
```

### Download file/directory

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o get -r /home/user/fun -d /home/user2 -k /home/key -c /home/ckpt
```

---

## Setup keys

- filerail uses AES-128 (CBC mode).
- It requires two paramaters IV (initialization vector) and key, both are of 128 bit length.
- First 48 bytes of key file consists of IV, and the remaining 48 bytes are keys.
- Pair of hex digits are separated by space, and last hex digit of each parameter is delimited by newline (\n).

### Example

#### Key file

```text
A9 51 D3 CC B5 F9 56 48 31 1B 5E 25 A9 E3 A1 DB
54 68 61 74 73 20 6D 79 20 4B 75 6E 67 20 46 75

```

---

# Dependencies

1. <a href="https://github.com/kuba--/zip">kuba--/zip</a> 
2. <a href="https://github.com/openssl/openssl">openssl/openssl</a>
3. <a href="https://github.com/msgpack/msgpack-c">msgpack/msgpack-c</a> 

---

# NOTE

- Windows and OS X not supported.