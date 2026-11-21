# Echo Service (RFC 862 )
TCP/UDP Echo Service implemented in C.

## Tests
Two tests were written to this code, the first (test_server_init()) checks if pollfds and clients variables were propely initialized.
The second (test_server_echo()) tries to connect to each server's socket, send a buffer and get the echo back.
## Macros

- LISTEN\_PORT (default: 7)
- LISTEN_BACKLOG (default: 50)
-  MAX_CLIENTS (default: 10)
- ENABLE\_IPV6 (default: ON)
- ENABLE\_UDP (default: ON)
- BUFFER_SIZE (default: 128)


## Building
Using CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release && \
    ctest --test-dir build
```


## Docker
 - Image size: 13.72MB (alpine:3.24.4)

```bash
docker build -t echo_server . && \
docker run -p 7:7 -p 7:7/udp echo_server
```


### Known issues:
-  Any UDP data that's greater than BUFFER_SIZE is cutted off.
