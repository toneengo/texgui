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
#find_package(msdfgen REQUIRED)

add_subdirectory(msdf-atlas-gen)

set(MSDF_ATLAS_NO_ARTERY_FONT=ON)
set(MSDF_ATLAS_USE_SKIA=OFF)
set(MSDF_ATLAS_USE_VCPKG=OFF)
set(MSDF_ATLAS_MSDFGEN_EXTERNAL=ON)

foreach(TARGET texgui)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT}")
    add_dependencies(${TARGET} msdf-atlas-gen)

    target_link_libraries(${TARGET} PRIVATE OpenGL::GL)
    target_link_libraries(${TARGET} PRIVATE glfw)
    target_link_libraries(${TARGET} PRIVATE Freetype::Freetype)
    target_link_libraries(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/msdf-atlas-gen/libmsdf-atlas-gen.a)
    target_link_libraries(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/msdf-atlas-gen/msdfgen/libmsdfgen-core.a)
    target_link_libraries(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/msdf-atlas-gen/msdfgen/libmsdfgen-ext.a)

    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen/msdfgen")
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
        add_dependencies(${X} msdf-atlas-gen)
        target_link_libraries(${X} PRIVATE glfw)
        target_link_libraries(${X} PRIVATE OpenGL::GL)
        target_link_libraries(${X} PRIVATE ${CMAKE_BINARY_DIR}/libtexgui.a)
        target_link_libraries(${X} PRIVATE ${CMAKE_BINARY_DIR}/msdf-atlas-gen/libmsdf-atlas-gen.a)
        target_link_libraries(${X} PRIVATE ${CMAKE_BINARY_DIR}/msdf-atlas-gen/libmsdf-atlas-gen.a)
        target_link_libraries(${X} PRIVATE ${CMAKE_BINARY_DIR}/msdf-atlas-gen/msdfgen/libmsdfgen-core.a)
        target_link_libraries(${X} PRIVATE ${CMAKE_BINARY_DIR}/msdf-atlas-gen/msdfgen/libmsdfgen-ext.a)

        target_link_libraries(${X} PRIVATE Freetype::Freetype)
        target_include_directories(${X} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_include_directories(${X} PRIVATE "include/")
    endforeach()

    add_subdirectory(examples)
endif()

add_subdirectory(src)
add_subdirectory(include/src)
