FROM ubuntu:20.04

# 環境変数の設定
ENV DEBIAN_FRONTEND=noninteractive
ENV XDG_RUNTIME_DIR=/run/user/0
ENV EGL_LOG_LEVEL=debug

# 必要なパッケージのインストール
RUN apt-get update && apt-get install -y \
    weston \
    xwayland \
    build-essential \
    cmake \
    wayland-protocols \
    libwayland-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    && ln -fs /usr/share/zoneinfo/Asia/Tokyo /etc/localtime \
    && dpkg-reconfigure --frontend noninteractive tzdata \
    && rm -rf /var/lib/apt/lists/*

# アプリケーションのソースコードをコンテナにコピー
COPY . /app

# 作業ディレクトリの設定
WORKDIR /app

# XDG_RUNTIME_DIR の作成と権限設定
RUN mkdir -p /run/user/0 && chmod 0700 /run/user/0

RUN bash internal_build.sh

CMD weston --backend=x11-backend.so & sleep 1 && ls  && ./build/my_wayland_app >./log/run.txt
