cmake_minimum_required(VERSION 3.0)
project(MyWaylandApp)

# ソースファイルを追加
add_executable(my_wayland_app src/main.cpp)

# リンクするライブラリを指定
target_link_libraries(my_wayland_app PRIVATE wayland-client wayland-egl EGL GL)

# ビルドディレクトリを設定
set(BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build")

# ログディレクトリを作成
file(MAKE_DIRECTORY "${BUILD_DIR}/log")

# ビルドログを出力
execute_process(COMMAND g++ -o my_wayland_app ../src/main.cpp -lwayland-client -lwayland-egl -lEGL -lGL OUTPUT_FILE "${BUILD_DIR}/log/build.txt" RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "Build failed, see ${BUILD_DIR}/log/build.txt for details")
endif()
