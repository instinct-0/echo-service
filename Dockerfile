FROM alpine:3 AS build

WORKDIR echo
COPY . .

RUN apk update
RUN apk add build-base cmake
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_IPV6=OFF && \
    cmake --build build --config Release && \
    ctest --test-dir build


FROM alpine:3.24.4
COPY --from=build echo/build/echo_server .

EXPOSE 7

RUN adduser -D echo_server
RUN chown echo_server:echo_server ./echo_server
USER echo_server

CMD "./echo_server"
