FROM gcc:9

RUN echo 'deb http://deb.debian.org/debian bullseye-backports main' >> /etc/apt/sources.list && \
    apt-get update -y && \
    apt-get install -y python3 golang-1.19 cmake ninja-build libgtest-dev wget
ENV PATH="/usr/lib/go-1.19/bin:$PATH"

ADD linux-binder.tar.gz /
WORKDIR /linux-binder
ENV ANDROID_BUILD_TOP=/linux-binder \
    ANDROID_HOST_OUT=/linux-binder/prebuilts/host/linux-x86
RUN CC=gcc CXX=g++ /linux-binder/frameworks/native/libs/binder/linux/build.sh

ENTRYPOINT [ "/linux-binder/frameworks/native/libs/binder/linux/build/binder_rpc_test" ]
