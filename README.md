WIP.
texgui is a minimal C++ library, which can use textures to render GUI elements for OpenGL games.
inspired by: nanogui, imgui

https://github.com/user-attachments/assets/3489f7f8-be36-4f4d-9c37-53cb1f855c83

texgui can be directly linked into your CMake project via adding this repository as a submodule.

#todo:
- plz remove glfw3 and freetype dependices if not building example
- markdown
- window ordering
# Build examples
These are instructions to build the standalone examples.
## Dependencies
- Ninja (optional)

## Linux (CMake)
To build the example:
1. Install dependencies using whatever package manager you have
```
archlinux
$ sudo pacman -S glfw3 freetype
ubuntu
$ sudo apt install glfw3 freetype
```
2. Clone this repository.
```
$ git clone https://github.com/toneengo/texgui.git
$ git submodule --init --recursive
$ cd .\texgui\
```
3. Use convenience Makefile.
```
$ make debug
```
4. run example program
```
$ build/Debug/example1
```

## Windows
### CMake (Ninja)
1. Install vcpkg. Set environment variable VCPKG_ROOT to your vcpkg directory.
2. Install all dependencies.
```
$ vcpkg install glfw3 freetype
```
3. Clone this repository.
```
$ git clone https://github.com/toneengo/texgui.git
$ git submodule --init --recursive
$ cd .\texgui\
```
4. Generate the build files. u can also use visual studio. Link the vcpkg cmake toolchain file.
```
$ mkdir build
$ cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$Env:VCPKG_ROOT\\scripts\\buildsystems\\vcpkg.cmake -G"Ninja"
$ cmake --build build
```
