cmake_minimum_required(VERSION 3.31)
project(install-apt VERSION 0.1.0 LANGUAGES C)

add_executable(install-apt install-apt.c)
install(TARGETS install-apt)
