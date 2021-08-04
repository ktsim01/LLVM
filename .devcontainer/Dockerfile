FROM ubuntu:20.04
RUN apt-get update
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install build-essential git wget llvm-11 flex bison gdb
RUN ln -s /usr/bin/llvm-config-11 /usr/bin/llvm-config