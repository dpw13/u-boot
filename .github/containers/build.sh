#!/bin/sh

REPO=$1

docker build . -t $REPO/aarch64-build:trixie
docker push $REPO/aarch64-build:trixie
