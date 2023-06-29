FROM debian:11
SHELL ["/bin/bash", "-c"]

RUN set -e; \
    apt-get update -y; \
    apt-get install -y sudo wget gnupg2;

RUN set -e; \
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc; \
    echo 'deb http://deb.debian.org/debian bullseye-backports main' >> /etc/apt/sources.list; \
    echo 'deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye main' >> /etc/apt/sources.list.d/llvm.list ; \
    echo 'deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye main' >> /etc/apt/sources.list.d/llvm.list ; \
    for llvm_version in 12 14 15 16; do \
        echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-$llvm_version main" >> /etc/apt/sources.list.d/llvm.list ; \
        echo "deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-$llvm_version main" >> /etc/apt/sources.list.d/llvm.list ; \
    done; \
    sudo apt-get update -y; \
    sudo apt-get install -y \
        g{cc,++}-10{,-aarch64-linux-gnu} \
        cmake \
        {clang{,-{format,tidy,tools}},lld}-1{1,2,3,4,5,6} \
        golang-1.19 \
        libc++{,abi}-14-dev \
        lib{fmt,gmock,gtest}-dev \
        ninja-build \
        python3

ENV PATH="/usr/lib/go-1.19/bin:$PATH"
