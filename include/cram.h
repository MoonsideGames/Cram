/* Cram - A texture atlas system in C
 *
 * Copyright (c) 2022 Evan Hemsley
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Evan "cosmonaut" Hemsley <evan@moonside.games>
 *
 */

#ifndef CRAM_H
#define CRAM_H

#ifdef _WIN32
#define CRAMAPI __declspec(dllexport)
#define CRAMCALL __cdecl
#else
#define CRAMAPI
#define CRAMCALL
#endif

#ifdef USE_SDL2
#include <SDL.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define CRAM_MAJOR_VERSION 0
#define CRAM_MINOR_VERSION 1
#define CRAM_PATCH_VERSION 0

#define WELLSPRING_COMPILED_VERSION ( \
	(CRAM_MAJOR_VERSION * 100 * 100) + \
	(CRAM_MINOR_VERSION * 100) + \
	(CRAM_PATCH_VERSION) \
)

CRAMAPI uint32_t Cram_LinkedVersion(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRAM_H */
