FROM ubuntu:latest

#
ARG DEBIAN_FRONTEND=noninteractive

# Install stuff.
RUN apt update -y; \
    apt upgrade -y; \
    apt install -y build-essential libboost-all-dev python3 git cmake alien \
                   libgflags-dev pkg-config asciidoc wget uuid-dev libjson-c-dev \
		   sudo;

# Install glog.
RUN git clone https://github.com/google/glog.git; \
    cd glog; \
    mkdir build; \
    cd build; \
    cmake ..; \
    make -j; \
    make install

# Install idxd-config.
RUN git clone https://github.com/intel/idxd-config.git; \
    cd idxd-config; \
    ./autogen.sh; \
    ./configure CFLAGS='-g -O2' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib; \
    make -j; \
    make install

# Install nasm.
RUN wget https://www.nasm.us/pub/nasm/releasebuilds/2.16.01/linux/nasm-2.16.01-0.fc36.x86_64.rpm; \
    alien nasm-2.16.01-0.fc36.x86_64.rpm; \
    dpkg -i nasm_2.16.01-1_amd64.deb

# Build benchmark.
RUN git clone --recursive https://github.com/barabanshek/IAA_benchmarking.git; \
    cd IAA_benchmarking; \
    mkdir build; \
    cd build; \
    cmake ..; \
    make -j;

CMD LD_LIBRARY_PATH=/usr/local/lib \
	 IAA_benchmarking/build/iaa_bench --benchmark_repetitions=1 --benchmark_min_time=1x --benchmark_format=csv --logtostderr