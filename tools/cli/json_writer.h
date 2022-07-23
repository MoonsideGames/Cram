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

#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include <string.h>
#include <stdint.h>

#define INITIAL_JSON_OUTPUT_CAPACITY 2048

typedef struct JsonBuilder
{
	char *string;
	size_t index;
	size_t capacity;
	size_t indentLevel;
} JsonBuilder;

JsonBuilder* JsonBuilder_Init()
{
	JsonBuilder *builder = malloc(sizeof(JsonBuilder));

	builder->string = malloc(INITIAL_JSON_OUTPUT_CAPACITY);
	builder->capacity = INITIAL_JSON_OUTPUT_CAPACITY;

	builder->string[0] = '\0';

	strcat(builder->string, "{\n");
	builder->indentLevel = 1;
	builder->index = 2;

	return builder;
}

void JsonBuilder_Internal_MaybeExpand(JsonBuilder *builder, size_t len)
{
	if (builder->capacity < builder->index + len)
	{
		builder->capacity = max(builder->index + len, builder->capacity * 2);
		builder->string = realloc(builder->string, builder->capacity);
	}
}

void JsonBuilder_Internal_Indent(JsonBuilder *builder)
{
	int32_t i;
	JsonBuilder_Internal_MaybeExpand(builder, builder->indentLevel);

	for (i = 0; i < builder->indentLevel; i += 1)
	{
		strcat(builder->string, "\t");
	}

	builder->index += builder->indentLevel;
}

void JsonBuilder_Internal_RemoveTrailingComma(JsonBuilder *builder)
{
	if (builder->string[builder->index - 2] == ',')
	{
		builder->index -= 2;
		builder->index += sprintf(&builder->string[builder->index], "\n");
	}
}

void JsonBuilder_AppendProperty(JsonBuilder *builder, char *propertyName, char *propertyString, uint8_t isString)
{
	JsonBuilder_Internal_Indent(builder);

	JsonBuilder_Internal_MaybeExpand(builder, strlen(propertyName) + 4);
	builder->index += sprintf(&builder->string[builder->index], "\"%s\": ", propertyName);

	if (isString)
	{
		JsonBuilder_Internal_MaybeExpand(builder, strlen(propertyString) + 4);
		builder->index += sprintf(&builder->string[builder->index], "\"%s\",\n", propertyString);
	}
	else
	{
		JsonBuilder_Internal_MaybeExpand(builder, strlen(propertyString) + 2);
		builder->index += sprintf(&builder->string[builder->index], "%s,\n", propertyString);
	}
}

void JsonBuilder_AppendStringProperty(JsonBuilder *builder, char *propertyName, char *value)
{
	JsonBuilder_AppendProperty(builder, propertyName, value, 1);
}

void JsonBuilder_AppendIntProperty(JsonBuilder *builder, char *propertyName, int32_t value)
{
	char buffer[65];
	itoa(value, buffer, 10);
	JsonBuilder_AppendProperty(builder, propertyName, buffer, 0);
}

void JsonBuilder_StartObject(JsonBuilder *builder)
{
	JsonBuilder_Internal_Indent(builder);
	JsonBuilder_Internal_MaybeExpand(builder, 2);
	builder->index += sprintf(&builder->string[builder->index], "{\n");
	builder->indentLevel += 1;
}

void JsonBuilder_EndObject(JsonBuilder *builder)
{
	JsonBuilder_Internal_RemoveTrailingComma(builder);

	builder->indentLevel -= 1;

	JsonBuilder_Internal_Indent(builder);
	JsonBuilder_Internal_MaybeExpand(builder, 3);
	builder->index += sprintf(&builder->string[builder->index], "},\n");
}

void JsonBuilder_StartArrayProperty(JsonBuilder *builder, char *propertyName)
{
	JsonBuilder_Internal_Indent(builder);
	JsonBuilder_Internal_MaybeExpand(builder, strlen(propertyName) + 6);
	builder->index += sprintf(&builder->string[builder->index], "\"%s\": [\n", propertyName);
	builder->indentLevel += 1;
}

void JsonBuilder_FinishArrayProperty(JsonBuilder *builder)
{
	JsonBuilder_Internal_RemoveTrailingComma(builder);

	builder->indentLevel -= 1;

	JsonBuilder_Internal_Indent(builder);
	JsonBuilder_Internal_MaybeExpand(builder, 3);
	builder->index += sprintf(&builder->string[builder->index], "],\n");
}

void JsonBuilder_Finish(JsonBuilder *builder)
{
	builder->indentLevel = 0;

	JsonBuilder_Internal_RemoveTrailingComma(builder);
	JsonBuilder_Internal_MaybeExpand(builder, 2);

	builder->index += sprintf(&builder->string[builder->index], "}\n");
}

void JsonBuilder_Destroy(JsonBuilder *builder)
{
	free(builder->string);
	free(builder);
}

#endif /* JSON_WRITER_H */
