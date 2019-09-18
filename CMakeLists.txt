cmake_minimum_required(VERSION 3.7.0)
project(cascade)
set(CMAKE_CXX_STANDARD 11)

file(GLOB DESCORE_SRCS src/descore/*.cpp)
file(GLOB CASCADE_SRCS src/cascade/*.cpp)

add_library(descore
    ${DESCORE_SRCS}
)

add_library(cascade
    ${CASCADE_SRCS}
)

target_include_directories(descore
    PUBLIC
    include/descore
)

target_include_directories(cascade
    PUBLIC
    include/
    include/cascade
)
target_link_libraries(cascade descore)

file(GLOB LIFE_SRCS examples/life/*.cpp)
add_executable(life
    ${LIFE_SRCS}
)
target_link_libraries(life cascade -lz -ltermcap -lpthread)