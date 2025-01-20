#linux only maybe macos too
debug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DBUILD_STATIC_LIBS=ON -DBUILD_EXAMPLE=ON -S . -B ./build/Debug -DMSDF_ATLAS_USE_SKIA=OFF
	cmake --build ./build/Debug --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

releaseexample:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=release -DBUILD_STATIC_LIBS=ON -DBUILD_EXAMPLE=ON -S . -B ./build/Release
	cmake --build ./build/Release --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

static:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DBUILD_STATIC_LIBS=ON -S . -B ./build/Release
	cmake --build ./build/Release --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

shared:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release _DBUILD_SHARED_LIBS=ON -S . -B ./build/Release
	cmake --build ./build/Release --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

both:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release _DBUILD_SHARED_LIBS=ON _DBUILD_STATIC_LIBS=ON -S . -B ./build/Release
	cmake --build ./build/Release --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
