
<p align="center"><img src="https://github.com/vi88i/filerail/blob/main/assets/filerail.png" alt="filerail"></p>

<p align="center"><b>Host a simple and cheap peer-to-peer file transfer program on your Linux server</b>.</p>

---

# Setup

## On server side

- Spin filerail server.

```bash
$ gcc -o filerail_server filerail_server.c
```

```bash
# usage: -v [-i ipv4 address] [-p port]
```

```bash
$ ./filerail_server -i 127.0.0.1 -p 8000
```

```text
options:
1. -u : usage
2. -v : verbose mode on
3. -i : IPv4 address of server
4. -p : port
```

## On client side

- Compile client side code.

```bash
$ gcc -o filerail_client filerail_client.c
```

```bash
# usage: -v [-i ipv4 address] [-p port] [-o operation] [-r resource path] [-d destination path]
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
```

### ping

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o ping
PONG
```

### Upload file/directory

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o put -r /home/user/a -d /home/user/fun
```

### Download file/directory

```bash
$ ./filerail_client -i 127.0.0.1 -p 8000 -o get -r /home/user/fun -d /home/user2
```

---

## Few notes

- Can handle transferring of both files and directories.
- For large files and directories it will be much better, if you zip them on your own (by means of zip utility ofc) to prevent timeout errors.

---

## Upcoming features

- Hash verification of file
- Adding SSL layer
- Checkpoint downloads/uploads

---

## Dependencies

- <a href="https://github.com/kuba--/zip">kuba--/zip</a> 

---

## Requirements

- Linux machine on server and client side with gcc compiler installed. (Sorry Windows and macOS fans)
