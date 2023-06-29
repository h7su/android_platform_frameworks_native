#!/bin/bash

set -e

../../release.sh
mv ../../linux-binder.tar.gz .
docker build -t linux-binder .
docker run -it linux-binder
