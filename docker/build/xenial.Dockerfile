FROM ubuntu:xenial
RUN  apt update
RUN  apt install -y cmake python libedit-dev g++ llvm-3.7-dev libncurses5-dev zlib1g-dev libreadline-dev lcov
CMD  mkdir -p /build && cd /build && cmake /src && make -j2 && make test
