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

if (BUILD_STATIC_LIBS)
    add_library(texgui STATIC)
endif()

if (BUILD_SHARED_LIBS)
    add_library(texgui SHARED)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm CONFIG REQUIRED)


foreach(TARGET texgui)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT}")

    target_link_libraries(${TARGET} PRIVATE OpenGL::GL)
    target_link_libraries(${TARGET} PRIVATE glfw)
    target_link_libraries(${TARGET} PRIVATE Freetype::Freetype)
    target_link_libraries(${TARGET} PRIVATE glm::glm)

    target_include_directories(${TARGET} PRIVATE "include/")
    target_include_directories(${TARGET} PRIVATE "resources/")
    target_include_directories(${TARGET} PRIVATE "src/")
endforeach(TARGET)

if (BUILD_EXAMPLE)
    add_executable(example1)
    list(APPEND Examples example1)

    add_custom_target(resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
    )

    foreach(X IN LISTS Examples)
        add_dependencies(${X} texgui)
        add_dependencies(${X} resources)
        target_link_directories(${X} PRIVATE ${CMAKE_BINARY_DIR})
        target_link_libraries(${X} PRIVATE texgui)
        target_include_directories(${X} PRIVATE "include/")
    endforeach()

    add_subdirectory(examples)
endif()

add_subdirectory(src)
add_subdirectory(include/src)
