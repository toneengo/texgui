cmake_minimum_required(VERSION 3.12)

if (WIN32)
    if( EXISTS $ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake )
        message(STATUS "Found $ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake; using it!")
        set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake
            CACHE STRING "Vcpkg toolchain file")
    endif()
endif(WIN32)

set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project("texgui")

add_library(texgui)
add_executable(example1)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm CONFIG REQUIRED)

add_subdirectory(src)
add_subdirectory(include/src)

add_subdirectory(examples)

foreach(TARGET texgui example1)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT}")

    target_link_libraries(${TARGET} PRIVATE OpenGL::GL)
    target_link_libraries(${TARGET} PRIVATE glfw)
    target_link_libraries(${TARGET} PRIVATE Freetype::Freetype)
    target_link_libraries(${TARGET} PRIVATE glm::glm)

    target_include_directories(${TARGET} PRIVATE "include/")
    target_include_directories(${TARGET} PRIVATE "resources/")
    target_include_directories(${TARGET} PRIVATE "src/")
endforeach(TARGET)

target_link_directories(example1 PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(example1 PRIVATE texgui)

add_custom_target(resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)

add_dependencies(example1 texgui)
add_dependencies(example1 resources)
