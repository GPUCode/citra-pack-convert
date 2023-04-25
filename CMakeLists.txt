cmake_minimum_required(VERSION 3.5)

project(citra-pack-convert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lodepng)
option(FMT_INSTALL "" ON)
add_subdirectory(fmt EXCLUDE_FROM_ALL)

add_executable(citra-pack-convert
    alignment.h
    bit_field.h
    color.h
    common_types.h
    etc1.cpp
    etc1.h
    main.cpp
    pixel_format.h
    swap.h
    texture_codec.h
    cityhash.cpp
    vector_math.h)

target_link_libraries(citra-pack-convert PRIVATE lodepng fmt)

install(TARGETS citra-pack-convert
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
