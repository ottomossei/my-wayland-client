#!/bin/bash

# ダングリングイメージの削除
# docker rmi $(docker images -f "dangling=true" -q)

# imageの作成
docker build -t wayland-container .

# Xサーバの許可を与える
xhost +

# コンテナを実行
docker run -it --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v .:/tmp/ \
    --name my-wayland \
    wayland-container
