cmake_minimum_required(VERSION 3.12)
project(tuner)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_executable(tuner "main.cpp" "tuner.cpp" "threadpool.cpp" "engines/toy.cpp" "engines/toy_tapered.cpp" "engines/skywall.cpp")

target_link_libraries(tuner PRIVATE Threads::Threads)
