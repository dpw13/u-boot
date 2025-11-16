FROM debian:trixie-slim

ENV \
    LANG="C.UTF-8" \
    LC_ALL="C.UTF-8" \
    DEBCONF_NONINTERACTIVE_SEEN="true" \
    DEBIAN_FRONTEND="noninteractive"

RUN apt-get update && apt-get install -y \
    nodejs \
    git \
    device-tree-compiler \
    build-essential \
    crossbuild-essential-arm64 \
    && apt-get clean \
    && rm -rf /var/lib/apt