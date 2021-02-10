
<p align="center"><img src="https://github.com/vi88i/filerail/blob/main/assets/filerail.png" alt="filerail"></p>

<p align="center"><b>Host a simple and cheap peer-to-peer file transfer program on your Linux server</b>.</p>

---

# Run

## On server side

```bash
$ gcc -o filerail_server filerail_server.c
$ ./filerail_server '<server ipv4 address>' '<server port>'
# Example
$ ./filerail_server 127.0.0.1 8000
```

- It starts a filerail server.

---

## On client side

```bash
$ gcc -o filerail_client filerail_client.c
$ ./filerail_client '<server ipv4 address>' '<server port>' '<command>' '<...args>'
```

### ping

```bash
$ ./filerail_client 127.0.0.1 8000 ping
PONG
```

### Upload file/directory

```bash
$ ./filerail_client 127.0.0.1 8000 put '<absolute pathname to source>' '<absolute pathname of target dir on server>'
# Example
$ ./filerail_client 127.0.0.1 8000 put /home/user/a /home/user/fun
```

### Download file/directory

```bash
$ ./filerail_client 127.0.0.1 8000 get '<absolute pathname to source on server>' '<absolute pathname of target dir on client>'
# Example
$ ./filerail_client 127.0.0.1 8000 get /home/user/fun /home/user2
```

## Dependencies

- <a href="https://github.com/kuba--/zip">kuba--/zip</a> 

## Requirements

- Linux machine on server and client side with gcc compiler installed. (Sorry Windows and macOS fans)
