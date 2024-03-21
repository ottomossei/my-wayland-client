# my-wayland-app

## Introduction

`my-wayland-app` is an open-source project demonstrating how to run GUI applications on Wayland within a Docker container. This project leverages Weston as the Wayland compositor backend and showcases the operation of a custom Wayland application named `my_wayland_app`.

## Features

- Running GUI applications on Wayland using Docker
- Utilizing Weston as the Wayland compositor
- Simple and easy-to-use setup

## Prerequisites

Before you begin, ensure you have the following installed on your host machine:

- Docker
- X server

## Getting Started

To get started with `my-wayland-app`, clone the repository and follow the steps below.

```bash
git clone https://github.com/yourusername/my-wayland-app.git
cd my-wayland-app
bash docker_run.sh
```

## Architecture
The following diagram provides an overview of the architecture:
![overview](./docs/overview.png)

## Results
Here's what you can expect as an output when you run the application. Note that the actual output may vary depending on the development stage.

![result](./docs/result.png)
