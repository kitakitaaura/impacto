#pragma once

#include <AL/al.h>
#include <AL/alc.h>

#if IMPACTO_OPENAL_HAVE_ALEXT
#include <AL/alext.h>
#else
// emscripten has these but not alext.h /shrug
#define AL_FORMAT_MONO_FLOAT32 0x10010
#define AL_FORMAT_STEREO_FLOAT32 0x10011
#endif

// Emscripten's AL headers don't define the calling-convention macro.
#ifndef AL_APIENTRY
#define AL_APIENTRY
#endif

#include "../audiocommon.h"
