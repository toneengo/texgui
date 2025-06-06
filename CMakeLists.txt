cmake_minimum_required(VERSION 3.12)

if (WIN32)
    if( EXISTS $ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake )
        message(STATUS "Found $ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake; using it!")
        set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake
            CACHE STRING "Vcpkg toolchain file")
    endif()
endif(WIN32)

if( EXISTS $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake )
    message(STATUS "Found $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake; using it!")
    set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
        CACHE STRING "Vcpkg toolchain file")
endif()

set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TEXGUI_BUILD_STATIC_LIBS "Build static libraries" ON)
option(TEXGUI_BUILD_STATIC_LIBS "Build shared libraries" OFF)
option(TEXGUI_BUILD_EXAMPLE "Build example applications" ON)

project("texgui")

if (TEXGUI_BUILD_SHARED_LIBS)
    add_library(texgui SHARED)
else()
    add_library(texgui STATIC)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

set(MSDF_ATLAS_NO_ARTERY_FONT ON)
set(MSDF_ATLAS_USE_SKIA OFF)
set(MSDF_ATLAS_MSDFGEN_EXTERNAL OFF)
if (WIN32)
    set(MSDF_ATLAS_USE_VCPKG ON)
endif()

add_subdirectory(msdf-atlas-gen)
add_subdirectory(VulkanMemoryAllocator)

#TODO: make a target for each backend instead of linking every single library at once
foreach(TARGET texgui)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT}")
    add_dependencies(${TARGET} msdf-atlas-gen)

    target_link_libraries(${TARGET} PRIVATE OpenGL::GL)
    target_link_libraries(${TARGET} PRIVATE Vulkan::Vulkan)
    target_link_libraries(${TARGET} PRIVATE glfw)
    target_link_libraries(${TARGET} PUBLIC Freetype::Freetype)

    target_link_libraries(${TARGET} PRIVATE glslang::glslang)
    target_link_libraries(${TARGET} PRIVATE glslang::glslang-default-resource-limits)

    target_link_libraries(${TARGET} PUBLIC GPUOpen::VulkanMemoryAllocator)

    target_link_libraries(${TARGET} PUBLIC msdf-atlas-gen::msdf-atlas-gen)
    target_link_libraries(${TARGET} PUBLIC msdfgen::msdfgen-core)
    target_link_libraries(${TARGET} PUBLIC msdfgen::msdfgen-ext)
    target_link_libraries(${TARGET} PUBLIC SDL3::SDL3)
    find_package(SDL3 CONFIG REQUIRED)

    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen/msdfgen")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include")
endforeach(TARGET)

if (TEXGUI_BUILD_EXAMPLE)
    add_executable(example1)
    list(APPEND Examples example1)

    add_custom_target(resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
    )

    foreach(X IN LISTS Examples)
        add_dependencies(${X} texgui)
        add_dependencies(${X} resources)
        target_link_libraries(${X} PRIVATE glfw)
        target_link_libraries(${X} PRIVATE OpenGL::GL)
        target_link_libraries(${X} PRIVATE Vulkan::Vulkan)
        target_link_libraries(${X} PRIVATE texgui)

        target_include_directories(${X} PRIVATE "include/")
    endforeach()

    add_subdirectory(examples)
endif()

add_subdirectory(src)
add_subdirectory(include/src)
