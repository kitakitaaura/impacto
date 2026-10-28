# Static wasm32 libraries for the threaded noberu web build (THREADS=1
# build.sh): everything is compiled with -pthread so it links into a
# shared-memory module. Meson ports ignore VCPKG_C_FLAGS, so build.sh also
# puts -pthread into EMCC_CFLAGS.
set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED EMSDK PATH EMCC_CFLAGS)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
set(VCPKG_BUILD_TYPE release)
set(VCPKG_C_FLAGS "-pthread")
set(VCPKG_CXX_FLAGS "-pthread")
set(VCPKG_LINKER_FLAGS "-pthread")
