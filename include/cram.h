/* Cram - A texture packing system in C
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

#include <stdint.h>

#ifdef _MSC_VER

#include <assert.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>

#endif /* _MSC_VER */

#ifdef _WIN32
#define SEPARATOR '\\'
#endif

#ifdef __unix__
#define SEPARATOR '/'
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define CRAM_MAJOR_VERSION 0
#define CRAM_MINOR_VERSION 1
#define CRAM_PATCH_VERSION 0

#define CRAM_COMPILED_VERSION ( \
	(CRAM_MAJOR_VERSION * 100 * 100) + \
	(CRAM_MINOR_VERSION * 100) + \
	(CRAM_PATCH_VERSION) \
)

CRAMAPI uint32_t Cram_LinkedVersion(void);

/* Type definitions */

typedef struct Cram_Context Cram_Context;

typedef struct Cram_ContextCreateInfo
{
	char *name;
	uint32_t maxDimension;
	int32_t padding;
	uint8_t trim;
} Cram_ContextCreateInfo;

typedef struct Cram_ImageData
{
	char *path;

	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;

	int32_t trimOffsetX;
	int32_t trimOffsetY;
	int32_t untrimmedWidth;
	int32_t untrimmedHeight;
} Cram_ImageData;

/* API definition */

CRAMAPI Cram_Context* Cram_Init(Cram_ContextCreateInfo *createInfo);

CRAMAPI void Cram_AddFile(Cram_Context *context, const char *path);

CRAMAPI int8_t Cram_Pack(Cram_Context *context);

CRAMAPI void Cram_GetPixelData(Cram_Context *context, uint8_t **pPixelData, int32_t *pWidth, int32_t *pHeight);
CRAMAPI void Cram_GetMetadata(Cram_Context *context, Cram_ImageData **pImage, int32_t *pImageCount);

CRAMAPI void Cram_Destroy(Cram_Context *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRAM_H */
