cmake_minimum_required(VERSION 3.21)
project(quarto_client C)

set(CMAKE_C_STANDARD 11)

include_directories(build/test/src)
include_directories(src)

add_executable(quarto_client
        build/test/src/client.c
        build/test/src/client.h
        build/test/src/config.c
        build/test/src/config.h
        build/test/src/main.c
        build/test/src/net.c
        build/test/src/net.h
        build/test/src/shm.c
        build/test/src/shm.h
        build/test/src/thinker.c
        build/test/src/thinker.h
        src/client.c
        src/client.h
        src/config.c
        src/config.h
        src/main.c
        src/net.c
        src/net.h
        src/shm.c
        src/shm.h
        src/thinker.c
        src/thinker.h)
