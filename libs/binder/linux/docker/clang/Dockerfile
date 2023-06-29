FROM debian:bookworm

RUN echo 'deb http://deb.debian.org/debian bookworm-backports main' >> /etc/apt/sources.list && \
    apt-get update -y && \
    apt-get install -y clang cmake ninja-build libgtest-dev

ADD linux-binder.tar.gz /
WORKDIR /linux-binder
ENV ANDROID_BUILD_TOP=/linux-binder \
    ANDROID_HOST_OUT=/linux-binder/prebuilts/host/linux-x86
RUN CC=clang CXX=clang++ /linux-binder/frameworks/native/libs/binder/linux/build.sh

ENTRYPOINT [ "/linux-binder/frameworks/native/libs/binder/linux/build/binder_rpc_test" ]
