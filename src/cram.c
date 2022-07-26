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

#include "cram.h"

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define INITIAL_DATA_CAPACITY 8
#define INITIAL_FREE_RECTANGLE_CAPACITY 16
#define INITIAL_DIMENSION 32

/* Structures */

typedef struct Rect
{
	int32_t x, y;
	int32_t w, h;
} Rect;

typedef struct Cram_Image Cram_Image;

struct Cram_Image
{
	char *name;
	Rect originalRect;
	Rect trimmedRect;
	Rect packedRect;
	Cram_Image *duplicateOf;
	uint8_t *pixels; /* Will be NULL if duplicateOf is not NULL! */
	size_t hash;
};

typedef struct Cram_Internal_Context
{
	char *name;

	int32_t padding;
	uint8_t trim;

	uint8_t *pixels;

	Cram_Image **images;
	int32_t imageCount;
	int32_t imageCapacity;

	Cram_ImageData *imageDatas;
	int32_t imageDataCount;

	int32_t maxDimension;

	int32_t packedWidth;
	int32_t packedHeight;
} Cram_Internal_Context;

typedef struct RectPackContext
{
	int32_t width;
	int32_t height;

	Rect *freeRectangles;
	int32_t freeRectangleCount;
	int32_t freeRectangleCapacity;

	Rect *newFreeRectangles;
	int32_t newFreeRectangleCount;
	int32_t newFreeRectangleCapacity;
} RectPackContext;

typedef struct PackScoreInfo
{
	int32_t score;
	int32_t secondaryScore;
	int32_t x;
	int32_t y;
} PackScoreInfo;

/* Pixel data functions */

/* https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2 */
static uint32_t Cram_Internal_NextPowerOfTwo(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

static uint8_t Cram_Internal_IsImageEqual(Cram_Image *a, Cram_Image *b)
{
	int32_t i;
	if (a->hash == b->hash && a->trimmedRect.w == b->trimmedRect.w && a->trimmedRect.h == b->trimmedRect.h)
	{
		for (i = 0; i < a->trimmedRect.w * a->trimmedRect.h * 4; i += 1)
		{
			if (a->pixels[i] != b->pixels[i])
			{
				return 0;
			}
		}

		return 1;
	}

	return 0;
}

static inline int32_t Cram_Internal_GetPixelIndex(int32_t x, int32_t y, int32_t width)
{
	return x + y * width;
}

static uint8_t Cram_Internal_IsRowClear(int32_t* pixels, int32_t rowIndex, int32_t width)
{
	int32_t i;

	for (i = 0; i < width; i += 1)
	{
		if ((pixels[Cram_Internal_GetPixelIndex(i, rowIndex, width)] & 0xFF) > 0)
		{
			return 0;
		}
	}

	return 1;
}

static uint8_t Cram_Internal_IsColumnClear(int32_t* pixels, int32_t columnIndex, int32_t width, int32_t height)
{
	int32_t i;

	for (i = 0; i < height; i += 1)
	{
		if ((pixels[Cram_Internal_GetPixelIndex(columnIndex, i, width)] & 0xFF) > 0)
		{
			return 0;
		}
	}

	return 1;
}

/* width and height of source and destination rects must be the same! */
static int8_t Cram_Internal_CopyPixels(
	uint32_t *dstPixels,
	uint32_t dstPixelWidth,
	uint32_t *srcPixels,
	uint32_t srcPixelWidth,
	Rect *dstRect,
	Rect *srcRect
) {
	int32_t i, j;
	int32_t dstPixelIndex, srcPixelIndex;

	if (dstRect->w != srcRect->w || dstRect->h != srcRect->h)
	{
		return -1;
	}

	for (i = 0; i < dstRect->w; i += 1)
	{
		for (j = 0; j < dstRect->h; j += 1)
		{
			dstPixelIndex = Cram_Internal_GetPixelIndex(i + dstRect->x, j + dstRect->y, dstPixelWidth);
			srcPixelIndex = Cram_Internal_GetPixelIndex(i + srcRect->x, j + srcRect->y, srcPixelWidth);
			dstPixels[dstPixelIndex] = srcPixels[srcPixelIndex];
		}
	}

	return 0;
}

/* Packing functions */

RectPackContext* Cram_Internal_InitRectPacker(int32_t width, int32_t height)
{
	RectPackContext *context = malloc(sizeof(RectPackContext));

	context->width = width;
	context->height = height;

	context->freeRectangleCapacity = INITIAL_FREE_RECTANGLE_CAPACITY;
	context->freeRectangles = malloc(sizeof(Rect) * context->freeRectangleCapacity);

	context->freeRectangles[0].x = 0;
	context->freeRectangles[0].y = 0;
	context->freeRectangles[0].w = width;
	context->freeRectangles[0].h = height;
	context->freeRectangleCount = 1;

	context->newFreeRectangleCapacity = INITIAL_FREE_RECTANGLE_CAPACITY;
	context->newFreeRectangles = malloc(sizeof(Rect) * context->freeRectangleCapacity);
	context->newFreeRectangleCount = 0;

	return context;
}

/* Uses the best area fit heuristic. */
/* TODO: make the heuristic configurable? */
void Cram_Internal_Score(
	RectPackContext *context,
	int32_t width,
	int32_t height,
	PackScoreInfo *scoreInfo
) {
	Rect *freeRect;
	int32_t areaFit;
	int32_t shortestSide;
	int32_t i;

	scoreInfo->score = INT32_MAX;
	scoreInfo->secondaryScore = INT32_MAX;

	for (i = 0; i < context->freeRectangleCount; i += 1)
	{
		freeRect = &context->freeRectangles[i];

		if (freeRect->w >= width && freeRect->h >= height)
		{
			areaFit = freeRect->w * freeRect->h - width * height;
			shortestSide = min(freeRect->w - width, freeRect->h - height);

			if (areaFit < scoreInfo->score || (areaFit == scoreInfo->score && shortestSide < scoreInfo->secondaryScore))
			{
				scoreInfo->score = areaFit;
				scoreInfo->secondaryScore = shortestSide;
				scoreInfo->x = freeRect->x;
				scoreInfo->y = freeRect->y;
			}
		}
	}
}

/* Check if a contains b */
static inline uint8_t Cram_Internal_Contains(Rect* a, Rect* b)
{
	return b->x >= a->x &&
		b->y >= a->y &&
		b->x + b->w <= a->x + a->w &&
		b->y + b->h <= a->y + a->h;
}

void Cram_Internal_PruneRects(RectPackContext* context)
{
	int32_t i, j;

	for (i = 0; i < context->freeRectangleCount; i += 1)
	{
		for (j = context->newFreeRectangleCount - 1; j >= 0; j -= 1)
		{
			if (Cram_Internal_Contains(&context->freeRectangles[i], &context->newFreeRectangles[j]))
			{
				/* plug the hole */
				context->newFreeRectangles[j] = context->newFreeRectangles[context->newFreeRectangleCount - 1];
				context->newFreeRectangleCount -= 1;
			}
		}
	}

	if (context->freeRectangleCapacity < context->freeRectangleCount + context->newFreeRectangleCount)
	{
		context->freeRectangleCapacity = max(context->freeRectangleCapacity * 2, context->freeRectangleCount + context->newFreeRectangleCount);
		context->freeRectangles = realloc(context->freeRectangles, sizeof(Rect) * context->freeRectangleCapacity);
	}

	for (i = 0; i < context->newFreeRectangleCount; i += 1)
	{
		context->freeRectangles[context->freeRectangleCount] = context->newFreeRectangles[i];
		context->freeRectangleCount += 1;
	}

	context->newFreeRectangleCount = 0;
}

static inline void Cram_Internal_AddNewFreeRect(RectPackContext *context, Rect rect)
{
	int32_t i;

	for (i = context->newFreeRectangleCount - 1; i >= 0; i -= 1)
	{
		if (Cram_Internal_Contains(&context->newFreeRectangles[i], &rect))
		{
			return;
		}

		if (Cram_Internal_Contains(&rect, &context->newFreeRectangles[i]))
		{
			context->newFreeRectangles[i] = context->newFreeRectangles[context->newFreeRectangleCount - 1];
			context->newFreeRectangleCount -= 1;
		}
	}

	if (context->newFreeRectangleCount == context->newFreeRectangleCapacity)
	{
		context->newFreeRectangleCapacity *= 2;
		context->newFreeRectangles = realloc(context->newFreeRectangles, sizeof(Rect) * context->newFreeRectangleCapacity);
	}

	context->newFreeRectangles[context->newFreeRectangleCount] = rect;
	context->newFreeRectangleCount += 1;
}

uint8_t Cram_Internal_SplitRect(RectPackContext *context, Rect *rect, Rect *freeRect)
{
	Rect newRect;

	/* Test intersection */
	if (	rect->x >= freeRect->x + freeRect->w ||
			rect->y >= freeRect->y + freeRect->h ||
			rect->x + rect->w <= freeRect->x ||
			rect->y + rect->h <= freeRect->y	)
	{
		return 0;
	}

	/* now we have new free rectangles! */
	/* NOTE: free rectangles can overlap. */

	if (rect->y < freeRect->y + freeRect->h && rect->y + rect->h > freeRect->y)
	{
		/* Left side */
		if (rect->x > freeRect->x && rect->x < freeRect->x + freeRect->w)
		{
			newRect = *freeRect;
			newRect.w = rect->x - freeRect->x;
			Cram_Internal_AddNewFreeRect(context, newRect);
		}

		/* Right side */
		if (rect->x + rect->w < freeRect->x + freeRect->w)
		{
			newRect = *freeRect;
			newRect.x = rect->x + rect->w;
			newRect.w = freeRect->x + freeRect->w - (rect->x + rect->w);
			Cram_Internal_AddNewFreeRect(context, newRect);
		}
	}

	if (rect->x < freeRect->x + freeRect->w && rect->x + rect->w > freeRect->x)
	{
		/* Top side */
		if (rect->y > freeRect->y && rect->y < freeRect->y + freeRect->h)
		{
			newRect = *freeRect;
			newRect.h = rect->y - freeRect->y;
			Cram_Internal_AddNewFreeRect(context, newRect);
		}

		/* Bottom side */
		if (rect->y + rect->h < freeRect->y + freeRect->h)
		{
			newRect = *freeRect;
			newRect.y = rect->y + rect->h;
			newRect.h = freeRect->y + freeRect->h - (rect->y + rect->h);
			Cram_Internal_AddNewFreeRect(context, newRect);
		}
	}

	return 1;
}

void Cram_Internal_PlaceRect(RectPackContext *context, Rect *rect)
{
	Rect *freeRect;
	int32_t i;

	for (i = context->freeRectangleCount - 1; i >= 0; i -= 1)
	{
		freeRect = &context->freeRectangles[i];

		if (Cram_Internal_SplitRect(context, rect, freeRect))
		{
			/* plug the hole */
			context->freeRectangles[i] = context->freeRectangles[context->freeRectangleCount - 1];
			context->freeRectangleCount -= 1;
		}
	}

	Cram_Internal_PruneRects(context);
}

/* Given rects with width and height, modifies rects with packed x and y positions. */
int8_t Cram_Internal_PackRects(RectPackContext *context, Rect *rects, int32_t numRects)
{
	Rect **rectsToPack = malloc(sizeof(Rect*) * numRects);
	int32_t rectsToPackCount = numRects;
	Rect *rectPtr;
	int32_t bestScore = INT32_MAX;
	int32_t bestSecondaryScore = INT32_MAX;
	PackScoreInfo scoreInfo;
	int32_t bestRectIndex, bestX, bestY;
	int32_t i, repeat;

	for (i = 0; i < numRects; i += 1)
	{
		rectsToPack[i] = &rects[i];
	}

	for (repeat = 0; repeat < numRects; repeat += 1)
	{
		bestScore = INT32_MAX;

		for (i = 0; i < rectsToPackCount; i += 1)
		{
			rectPtr = rectsToPack[i];

			 Cram_Internal_Score(context, rectPtr->w, rectPtr->h, &scoreInfo);

			if (scoreInfo.score < bestScore || (scoreInfo.score == bestScore && scoreInfo.secondaryScore < bestSecondaryScore))
			{
				bestScore = scoreInfo.score;
				bestSecondaryScore = scoreInfo.secondaryScore;
				bestRectIndex = i;
				bestX = scoreInfo.x;
				bestY = scoreInfo.y;
			}
		}

		if (bestScore == INT32_MAX)
		{
			/* doesn't fit! abort! */
			return -1;
		}

		rectPtr = rectsToPack[bestRectIndex];
		rectPtr->x = bestX;
		rectPtr->y = bestY;
		Cram_Internal_PlaceRect(context, rectPtr);

		/* plug the hole */
		rectsToPack[bestRectIndex] = rectsToPack[rectsToPackCount - 1];
		rectsToPackCount -= 1;
	}

	free(rectsToPack);
	return 0;
}

/* API functions */

uint32_t Cram_LinkedVersion(void)
{
	return CRAM_COMPILED_VERSION;
}

Cram_Context* Cram_Init(Cram_ContextCreateInfo *createInfo)
{
	Cram_Internal_Context *context = malloc(sizeof(Cram_Internal_Context));

	context->name = strdup(createInfo->name);

	context->padding = createInfo->padding;
	context->trim = createInfo->trim;

	context->images = malloc(INITIAL_DATA_CAPACITY * sizeof(Cram_Image*));
	context->imageCapacity = INITIAL_DATA_CAPACITY;
	context->imageCount = 0;

	context->pixels = NULL;
	context->imageDatas = NULL;
	context->imageDataCount = 0;

	context->packedWidth = 0;
	context->packedHeight = 0;

	context->maxDimension = createInfo->maxDimension;

	return (Cram_Context*) context;
}

static char* Cram_Internal_GetImageName(const char *path)
{
	char *lastSeparator = strrchr(path, SEPARATOR) + 1;
	size_t returnBytes = strlen(lastSeparator) + 1;
	char *name = malloc(returnBytes);
	int32_t i;

	for (i = 0; i < returnBytes; i += 1)
	{
		name[i] = lastSeparator[i];
	}

	return name;
}

void Cram_AddFile(Cram_Context *context, const char *path)
{
	Cram_Internal_Context *internalContext = (Cram_Internal_Context*) context;
	Cram_Image *image;
	uint8_t *pixels;
	int32_t leftTrim, topTrim, rightTrim, bottomTrim;
	int32_t width, height, numChannels;
	int32_t i;

	if (internalContext->imageCapacity == internalContext->imageCount)
	{
		internalContext->imageCapacity *= 2;
		internalContext->images = realloc(internalContext->images, internalContext->imageCapacity * sizeof(Cram_Image*));
	}

	image = malloc(sizeof(Cram_Image));

	image->name = Cram_Internal_GetImageName(path);

	pixels = stbi_load(
		path,
		&width,
		&height,
		&numChannels,
		STBI_rgb_alpha
	);

	image->originalRect.x = 0;
	image->originalRect.y = 0;
	image->originalRect.w = width;
	image->originalRect.h = height;

	/* Check for trim */
	if (internalContext->trim)
	{
		topTrim = 0;
		for (i = 0; i < height; i += 1)
		{
			if (!Cram_Internal_IsRowClear((uint32_t*) pixels, i, width))
			{
				topTrim = i;
				break;
			}
		}

		leftTrim = 0;
		for (i = 0; i < width; i += 1)
		{
			if (!Cram_Internal_IsColumnClear((uint32_t*) pixels, i, width, height))
			{
				leftTrim = i;
				break;
			}
		}

		bottomTrim = height;
		for (i = height - 1; i >= topTrim; i -= 1)
		{
			if (!Cram_Internal_IsRowClear((uint32_t*) pixels, i, width))
			{
				bottomTrim = i + 1;
				break;
			}
		}

		rightTrim = width;
		for (i = width - 1; i >= leftTrim; i -= 1)
		{
			if (!Cram_Internal_IsColumnClear((uint32_t*) pixels, i, width, height))
			{
				rightTrim = i + 1;
				break;
			}
		}

		image->trimmedRect.x = leftTrim;
		image->trimmedRect.y = topTrim;
		image->trimmedRect.w = rightTrim - leftTrim;
		image->trimmedRect.h = bottomTrim - topTrim;
	}
	else
	{
		image->trimmedRect = image->originalRect;
	}

	/* copy and free source pixels */
	image->pixels = malloc(image->trimmedRect.w * image->trimmedRect.h * 4);

	Rect dstRect;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.w = image->trimmedRect.w;
	dstRect.h = image->trimmedRect.h;
	Cram_Internal_CopyPixels((uint32_t*) image->pixels, image->trimmedRect.w, (uint32_t*) pixels, width, &dstRect, &image->trimmedRect);
	stbi_image_free(pixels);

	/* hash */
	image->hash = stbds_hash_bytes(image->pixels, image->trimmedRect.w * image->trimmedRect.h * 4, 0);

	/* check if this is a duplicate */
	image->duplicateOf = NULL;
	for (i = 0; i < internalContext->imageCount; i += 1)
	{
		if (!internalContext->images[i]->duplicateOf)
		{
			if (Cram_Internal_IsImageEqual(image, internalContext->images[i]))
			{
				/* this is duplicate data! */
				image->duplicateOf = internalContext->images[i];
				free(image->pixels);
				image->pixels = NULL;
				break;
			}
		}
	}

	internalContext->images[internalContext->imageCount] = image;
	internalContext->imageCount += 1;
}

int8_t Cram_Pack(Cram_Context *context)
{
	RectPackContext *rectPackContext;
	Cram_Internal_Context *internalContext = (Cram_Internal_Context*) context;
	Rect *packerRects;
	uint32_t numRects = 0;
	Rect *packerRect;
	Rect dstRect, srcRect;
	Cram_Image *image;
	uint8_t increaseX = 1;
	int32_t i;

	internalContext->imageDataCount = internalContext->imageCount;
	internalContext->imageDatas = realloc(internalContext->imageDatas, sizeof(Cram_ImageData) * internalContext->imageDataCount);

	rectPackContext = Cram_Internal_InitRectPacker(INITIAL_DIMENSION, INITIAL_DIMENSION);

	for (i = 0; i < internalContext->imageCount; i += 1)
	{
		if (!internalContext->images[i]->duplicateOf)
		{
			numRects += 1;
		}
	}

	packerRects = malloc(sizeof(Rect) * numRects);

	numRects = 0;
	for (i = 0; i < internalContext->imageCount; i += 1)
	{
		if (!internalContext->images[i]->duplicateOf)
		{
			packerRect = &packerRects[numRects];

			packerRect->w = internalContext->images[i]->trimmedRect.w + internalContext->padding;
			packerRect->h = internalContext->images[i]->trimmedRect.h + internalContext->padding;

			numRects += 1;
		}
	}

	/* If packing fails, increase a dimension by power of 2 and retry until we hit max dimensions. */
	while (Cram_Internal_PackRects(rectPackContext, packerRects, numRects) < 0)
	{
		if (increaseX)
		{
			rectPackContext->width *= 2;
			increaseX = 0;
		}
		else
		{
			rectPackContext->height *= 2;
			increaseX = 1;
		}

		rectPackContext->freeRectangles[0].x = 0;
		rectPackContext->freeRectangles[0].y = 0;
		rectPackContext->freeRectangles[0].w = rectPackContext->width;
		rectPackContext->freeRectangles[0].h = rectPackContext->height;
		rectPackContext->freeRectangleCount = 1;

		rectPackContext->newFreeRectangleCount = 0;

		if (rectPackContext->width > internalContext->maxDimension || rectPackContext->height > internalContext->maxDimension)
		{
			break;
		}
	}

	if (rectPackContext->width > internalContext->maxDimension || rectPackContext->height > internalContext->maxDimension)
	{
		/* Can't pack into max dimensions, abort! */
		return -1;
	}

	numRects = 0;
	for (i = 0; i < internalContext->imageCount; i += 1)
	{
		if (!internalContext->images[i]->duplicateOf)
		{
			packerRect = &packerRects[numRects];

			internalContext->images[i]->packedRect.x = packerRect->x;
			internalContext->images[i]->packedRect.y = packerRect->y;
			internalContext->images[i]->packedRect.w = internalContext->images[i]->trimmedRect.w;
			internalContext->images[i]->packedRect.h = internalContext->images[i]->trimmedRect.h;

			numRects += 1;
		}
	}

	internalContext->packedWidth = rectPackContext->width;
	internalContext->packedHeight = rectPackContext->height;

	internalContext->pixels = realloc(internalContext->pixels, internalContext->packedWidth  * internalContext->packedHeight * 4);
	memset(internalContext->pixels, 0, internalContext->packedWidth * internalContext->packedHeight * 4);

	for (i = 0; i < internalContext->imageCount; i += 1)
	{
		if (!internalContext->images[i]->duplicateOf)
		{
			dstRect.x = internalContext->images[i]->packedRect.x;
			dstRect.y = internalContext->images[i]->packedRect.y;
			dstRect.w = internalContext->images[i]->trimmedRect.w;
			dstRect.h = internalContext->images[i]->trimmedRect.h;

			srcRect.x = 0;
			srcRect.y = 0;
			srcRect.w = internalContext->images[i]->trimmedRect.w;
			srcRect.h = internalContext->images[i]->trimmedRect.h;

			Cram_Internal_CopyPixels(
				(uint32_t*) internalContext->pixels,
				internalContext->packedWidth,
				(uint32_t*) internalContext->images[i]->pixels,
				internalContext->images[i]->trimmedRect.w,
				&dstRect,
				&srcRect
			);
		}

		if (internalContext->images[i]->duplicateOf)
		{
			image = internalContext->images[i]->duplicateOf;
		}
		else
		{
			image = internalContext->images[i];
		}

		internalContext->imageDatas[i].x = image->packedRect.x;
		internalContext->imageDatas[i].y = image->packedRect.y;
		internalContext->imageDatas[i].width = image->trimmedRect.w;
		internalContext->imageDatas[i].height = image->trimmedRect.h;

		internalContext->imageDatas[i].trimOffsetX = image->trimmedRect.x - image->originalRect.x;
		internalContext->imageDatas[i].trimOffsetY = image->trimmedRect.y - image->originalRect.y;
		internalContext->imageDatas[i].untrimmedWidth = image->originalRect.w;
		internalContext->imageDatas[i].untrimmedHeight = image->originalRect.h;

		internalContext->imageDatas[i].name = strdup(internalContext->images[i]->name);
	}

	free(packerRects);

	return 0;
}

void Cram_GetPixelData(Cram_Context *context, uint8_t **pPixels, int32_t *pWidth, int32_t *pHeight)
{
	Cram_Internal_Context *internalContext = (Cram_Internal_Context*) context;
	*pPixels = internalContext->pixels;
	*pWidth = internalContext->packedWidth;
	*pHeight = internalContext->packedHeight;
}

void Cram_GetMetadata(Cram_Context *context, Cram_ImageData **pImage, int32_t *pImageCount)
{
	Cram_Internal_Context *internalContext = (Cram_Internal_Context*) context;

	*pImage = internalContext->imageDatas;
	*pImageCount = internalContext->imageDataCount;
}

void Cram_Destroy(Cram_Context *context)
{
	Cram_Internal_Context *internalContext = (Cram_Internal_Context*) context;
	int32_t i;

	if (internalContext->pixels != NULL)
	{
		free(internalContext->pixels);
	}

	for (i = 0; i < internalContext->imageCount; i += 1)
	{
		if (!internalContext->images[i]->duplicateOf)
		{
			free(internalContext->images[i]->pixels);
		}

		free(internalContext->images[i]->name);
		free(internalContext->images[i]);
	}

	free(internalContext->name);
	free(internalContext->images);
	free(internalContext->imageDatas);
	free(internalContext);
}
