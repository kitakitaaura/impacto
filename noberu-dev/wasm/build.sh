#!/bin/sh
# Build impacto for the browser (noberu web). Output: noberu-dev/wasm/out/
#   noberu-dev/wasm/build.sh            # configure + build
#   THREADS=1 noberu-dev/wasm/build.sh  # pthreads + FFmpeg movies + libass
#                                       # subtitles -> out-mt/
#                                       # (needs a cross-origin isolated page)
# Needs ~/emsdk and the wasm deps (vcpkg install --triplet wasm32-emscripten-noberu,
# run automatically by the configure step through the manifest in this folder).
set -e
WASM=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$WASM/../.." && pwd)
. "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
export EMCC_CFLAGS="-DHB_NO_PRAGMA_GCC_DIAGNOSTIC_ERROR"
VCPKG_ROOT=${VCPKG_ROOT:-$ROOT/../third_party/vcpkg}
if [ "${THREADS:-0}" = 1 ]; then
  # Every object, vcpkg ports included, must be built with -pthread.
  export EMCC_CFLAGS="$EMCC_CFLAGS -pthread"
  BUILD="$ROOT/impacto-build/wasm-mt"
  OUT="$WASM/out-mt"
  set -- -DVCPKG_TARGET_TRIPLET=wasm32-emscripten-noberu-mt \
    -DVCPKG_INSTALLED_DIR="$WASM/vcpkg_installed-mt" \
    -DVCPKG_MANIFEST_FEATURES=movies -DIMPACTO_HAVE_THREADS=ON \
    -UIMPACTO_DISABLE_LIBASS
else
  BUILD="$ROOT/impacto-build/wasm"
  OUT="$WASM/out"
  set -- -DVCPKG_TARGET_TRIPLET=wasm32-emscripten-noberu \
    -DVCPKG_INSTALLED_DIR="$WASM/vcpkg_installed" \
    -DIMPACTO_DISABLE_FFMPEG=ON -DIMPACTO_HAVE_THREADS=OFF \
    -DIMPACTO_DISABLE_LIBASS=ON
fi
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
  -DVCPKG_OVERLAY_TRIPLETS="$WASM/triplets" \
  -DVCPKG_MANIFEST_DIR="$WASM" \
  -DIMPACTO_DISABLE_IMGUI=ON \
  -DIMPACTO_WARNINGS=OFF "$@"
cmake --build "$BUILD" --target impacto
mkdir -p "$OUT"
cp "$BUILD"/impacto.js "$BUILD"/impacto.wasm "$BUILD"/impacto.data "$OUT/"
cp "$WASM"/web/* "$OUT/"
echo "built $OUT"
