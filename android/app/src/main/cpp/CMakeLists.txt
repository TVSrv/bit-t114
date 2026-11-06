# Ядро игры с платы — то же самое, что в прошивке, — рисует питомца в приложении.
cmake_minimum_required(VERSION 3.22.1)
project(bitcore CXX)
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core)
add_library(bitcore SHARED
    bitjni.cpp
    ${CORE}/gfx.cpp ${CORE}/fonts.cpp ${CORE}/art.cpp ${CORE}/game.cpp ${CORE}/scenes.cpp
    ${CORE}/extras.cpp ${CORE}/meet.cpp ${CORE}/nick.cpp ${CORE}/remote.cpp ${CORE}/gps.cpp ${CORE}/util.cpp ${CORE}/artdata.cpp ${CORE}/rooms.cpp)
target_include_directories(bitcore PRIVATE ${CORE})
target_compile_options(bitcore PRIVATE -O2 -fno-exceptions -fno-rtti)
