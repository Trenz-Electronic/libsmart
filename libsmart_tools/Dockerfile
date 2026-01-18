# libsmart build environment
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Trenz Electronic GmbH

FROM debian:bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    libcrack2-dev \
    && rm -rf /var/lib/apt/lists/*

#mount ..:/work
WORKDIR /work
