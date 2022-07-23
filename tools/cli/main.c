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
#include "dirent.h"
#include "json_writer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAX_DIR_LENGTH 2048

static Cram_Context *context;

static const char* GetFilenameExtension(const char *filename)
{
	const char *dot = strrchr(filename, '.');
	if (!dot || dot == filename)
	{
		return "";
	}
	return dot + 1;
}

/* Mostly taken from K&R C 2nd edition page 182 */
static void dirwalk(char *dir)
{
	dirent *dp;
	DIR *dfd;
	char subname[2048];

	if ((dfd = opendir(dir)) == NULL)
	{
		fprintf(stderr, "Can't open %s\n", dir);
		return;
	}

	while ((dp = readdir(dfd)) != NULL)
	{
		if (	strcmp(dp->d_name, ".") == 0 ||
				strcmp(dp->d_name, "..") == 0
		)
		{
			continue;
		}

		sprintf(subname, "%s%c%s", dir, SEPARATOR, dp->d_name);

		if (dp->d_type == DT_DIR)
		{
			dirwalk(subname);
		}
		else
		{
			if (strcmp(GetFilenameExtension(subname), "png") == 0)
			{
				Cram_AddFile(context, subname);
			}
			else
			{
				fprintf(stdout, "skipping %s\n", subname);
			}
		}
	}

	closedir(dfd);
}

/* TODO: command line options */

void print_help()
{
	fprintf(stdout, "Usage: cram input_dir output_dir atlas_name [--padding padding_value] [--notrim] [--dimension max_dimension]");
}

uint8_t check_dir_exists(char *path)
{
	DIR *dir = opendir(path);
	if (dir) {
		closedir(dir);
		return 1;
	} else {
		return 0;
	}
}

int main(int argc, char *argv[])
{
	Cram_ContextCreateInfo createInfo;
	uint8_t *pixelData;
	uint32_t width;
	uint32_t height;
	char *arg;
	char *inputDirPath = NULL;
	char *outputDirPath = NULL;
	char *imageOutputFilename;
	char *metadataFilename;
	JsonBuilder *jsonBuilder;
	Cram_ImageData *imageDatas;
	int32_t imageCount;
	int32_t i;

	/* Set defaults */
	createInfo.padding = 0;
	createInfo.trim = 1;
	createInfo.maxDimension = 8192;
	createInfo.name = NULL;

	if (argc < 2)
	{
		print_help();
		return 1;
	}

	for (i = 1; i < argc; i += 1)
	{
		arg = argv[i];

		if (strcmp(arg, "--padding") == 0)
		{
			i += 1;
			createInfo.padding = atoi(argv[i]);
			if (createInfo.padding < 0)
			{
				fprintf(stderr, "Padding must be equal to or greater than 0!");
				return 1;
			}
		}
		else if (strcmp(arg, "--notrim") == 0)
		{
			createInfo.trim = 0;
		}
		else if (strcmp(arg, "--dimension") == 0)
		{
			i += 1;
			createInfo.maxDimension = atoi(argv[i]);
			if (createInfo.maxDimension < 0 || createInfo.maxDimension > 8192)
			{
				fprintf(stderr, "Padding must be between 0 and 8192!");
				return 1;
			}
		}
		else if (strcmp(arg, "--help") == 0)
		{
			print_help();
			return 0;
		}
		else
		{
			if (inputDirPath == NULL)
			{
				inputDirPath = arg;
			}
			else if (outputDirPath == NULL)
			{
				outputDirPath = arg;
			}
			else if (createInfo.name == NULL)
			{
				createInfo.name = arg;
			}
		}
	}

	if (inputDirPath == NULL || createInfo.name == NULL)
	{
		print_help();
		return 1;
	}

	/* check that dirs exist */
	if (!check_dir_exists(inputDirPath))
	{
		fprintf(stderr, "Input directory not found!");
		return 1;
	}

	if (!check_dir_exists(outputDirPath))
	{
		fprintf(stderr, "Output directory not found!");
		return 1;
	}

	context = Cram_Init(&createInfo);

	dirwalk(inputDirPath);

	if (Cram_Pack(context) < 0)
	{
		fprintf(stderr, "Not enough room! Packing aborted!");
		return 1;
	}

	/* output pixel data */

	Cram_GetPixelData(context, &pixelData, &width, &height);
	imageOutputFilename = malloc(strlen(createInfo.name) + 5);
	strcpy(imageOutputFilename, createInfo.name);
	strcat(imageOutputFilename, ".png");

	stbi_write_png(
		imageOutputFilename,
		width,
		height,
		4,
		pixelData,
		width * 4
	);

	/* output json */

	Cram_GetMetadata(context, &imageDatas, &imageCount);

	jsonBuilder = JsonBuilder_Init();
	JsonBuilder_AppendStringProperty(jsonBuilder, "Name", createInfo.name);
	JsonBuilder_AppendIntProperty(jsonBuilder, "Width", width);
	JsonBuilder_AppendIntProperty(jsonBuilder, "Height", height);
	JsonBuilder_StartArrayProperty(jsonBuilder, "Images");
	for (i = 0; i < imageCount; i += 1)
	{
		JsonBuilder_StartObject(jsonBuilder);
		JsonBuilder_AppendStringProperty(jsonBuilder, "Name", imageDatas[i].name);
		JsonBuilder_AppendIntProperty(jsonBuilder, "X", imageDatas[i].x);
		JsonBuilder_AppendIntProperty(jsonBuilder, "Y", imageDatas[i].y);
		JsonBuilder_AppendIntProperty(jsonBuilder, "W", imageDatas[i].width);
		JsonBuilder_AppendIntProperty(jsonBuilder, "H", imageDatas[i].height);
		JsonBuilder_AppendIntProperty(jsonBuilder, "TrimOffsetX", imageDatas[i].trimOffsetX);
		JsonBuilder_AppendIntProperty(jsonBuilder, "TrimOffsetY", imageDatas[i].trimOffsetY);
		JsonBuilder_AppendIntProperty(jsonBuilder, "UntrimmedWidth", imageDatas[i].untrimmedWidth);
		JsonBuilder_AppendIntProperty(jsonBuilder, "UntrimmedHeight", imageDatas[i].untrimmedHeight);
		JsonBuilder_EndObject(jsonBuilder);
	}
	JsonBuilder_FinishArrayProperty(jsonBuilder);
	JsonBuilder_Finish(jsonBuilder);

	metadataFilename = malloc(strlen(createInfo.name) + 6);
	strcpy(metadataFilename, createInfo.name);
	strcat(metadataFilename, ".json");

	FILE *jsonOutput = fopen(metadataFilename, "w");
	if (!jsonOutput)
	{
		fprintf(stderr, "Could not open JSON file for writing!");
		return 1;
	}

	fprintf(jsonOutput, jsonBuilder->string);

	JsonBuilder_Destroy(jsonBuilder);
	fclose(jsonOutput);

	free(imageOutputFilename);
	free(metadataFilename);
	Cram_Destroy(context);

	return 0;
}
