# Static wasm32 libraries for the noberu web build (single-threaded: the engine
# runs on the main thread and awaits archive reads through ASYNCIFY).
set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED EMSDK PATH EMCC_CFLAGS)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
set(VCPKG_BUILD_TYPE release)
# emsdk's newer clang trips harfbuzz's warning-as-error pragmas
# (-Wunused-template). Meson ports ignore VCPKG_CXX_FLAGS here, so build.sh
# exports EMCC_CFLAGS=-DHB_NO_PRAGMA_GCC_DIAGNOSTIC_ERROR, which emcc applies to
# every compile.
