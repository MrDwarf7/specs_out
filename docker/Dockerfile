# Generic C development container
#
# Pinned to gcc-14 for full C23 support (-std=gnu23).
# Adjust GCC_VERSION arg for your needs.
#
# Usage:
#   docker build -t specs_out-dev -f docker/Dockerfile .
#   docker run --rm -v $(pwd):/project -w /project specs_out-dev make a

ARG GCC_VERSION=14
FROM gcc:${GCC_VERSION}-bookworm

RUN apt-get update && apt-get install -y \
    make \
    cppcheck \
    clang-tools \
    bash \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /project

CMD ["bash"]
