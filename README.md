
<p align="center"><img src="https://github.com/vi88i/filerail/blob/main/assets/filerail.png" alt="filerail"></p>

<p align="center"><b>Host a simple and cheap peer-to-peer file transfer program on your Linux server</b>.</p>

---

# How filerail works ?

- Client establishes TCP connection with server.
- Client exchanges messages by using custom protocol, to perform various checks for given operation.
- To send resource, the sender zips the resource, encrypts it and advertises the hash of zipped file.
- The receiver decrypts, verifies the hash and only then unzip the downloaded file.

# Setup

## On server side

- Spin filerail server.

```bash
$ gcc -o filerail_server filerail_server.c -lssl -lcrypto -Wall
```

```bash
# usage: -v [-i ipv4 address] [-p port] [-k key directory]
```

```bash
$ ./filerail_server -v -i 127.0.0.1 -p 8000 -k /home/key
```

```text
options:
1. -u : usage
2. -v : verbose mode on
3. -i : IPv4 address of server
4. -p : port
5. -k : key directory (requires absolute path to key directory)
```

## On client side

- Compile client side code.

```bash
$ gcc -o filerail_server filerail_server.c -lssl -lcrypto -Wall
```

```bash
# usage: -v [-i ipv4 address] [-p port] [-o operation] [-r resource path] [-d destination path] [-k key directory]
```

```text
options:
1. -u : usage
2. -v : verbose mode on
3. -i : IPv4 address of server
4. -p : port
5. -o : operation {put, get, ping}
6. -r : resource path (requires absolute path to resource)
7. -d : destination path (requires absolute path to destination)
8. -k : key directory (requires absolute path to key directory)
```

### ping

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o ping
PONG
```

### Upload file/directory

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o put -r /home/user/a -d /home/user/fun -k /home/key
```

### Download file/directory

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o get -r /home/user/fun -d /home/user2 -k /home/key
```

---

## Setup keys

- filerail uses AES (CTR mode).
- It requires two keys stored in two separate files (in single directory), and both keys are of 128-bit length.
- Pair of hex digits are separated by space, and last hex digit is delimited by newline (\n).

### Example

#### Key file

```text
54 68 61 74 73 20 6D 79 20 4B 75 6E 67 20 46 75

```

#### Nonce file

```text
A9 51 D3 CC B5 F9 56 48 31 1B 5E 25 A9 E3 A1 DB

```

---

## Upcoming features

- Save checkpoints while download/upload

---

## Dependencies

- <a href="https://github.com/kuba--/zip">kuba--/zip</a> (included in deps) 
- <a href="https://github.com/openssl/openssl">openssl</a> version: 3.0.0-alpha12-dev (not included in deps, you got to install it)

---

## Requirements

- Linux machine on server and client side with gcc compiler installed. (Sorry Windows and macOS fans)
