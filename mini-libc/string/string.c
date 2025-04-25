// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>

char *strcpy(char *destination, const char *source)
{
	int i;
	for (i = 0; *(source + i); ++i)
		*(destination + i) = *(source + i);
	*(destination + i) = *(source + i);

	return destination;
}

char *strncpy(char *destination, const char *source, size_t len)
{
	size_t i;
	for (i = 0; i < len && *(source + i) != '\0'; ++i)
		*(destination + i) = *(source + i);
	while (i < len) {
		*(destination + i) = '\0';
		++i;
	}

	return destination;
}

char *strcat(char *destination, const char *source)
{
	size_t i, j;
	for (i = 0; *(destination + i) != '\0'; ++i) {}
	for (j = 0; *(source + j) != '\0'; ++j) {
		*(destination + i) = *(source + j);
		++i;
	}
	*(destination + i) = '\0';
	return destination;
}

char *strncat(char *destination, const char *source, size_t len)
{
	size_t i, j;
	for (i = 0; *(destination + i) != '\0'; ++i) {}
	for (j = 0; j < len && *(source + j) != '\0'; ++j) {
		*(destination + i) = *(source + j);
		++i;
	}
	*(destination + i) = '\0';
	return destination;
}

int strcmp(const char *str1, const char *str2)
{
	size_t i = 0;
	int flag = 0;
	while (*(str1 + i) != '\0' || *(str2 + i) != '\0') {
		flag = *(str1 + i) - *(str2 + i);
		if (flag != 0)
			return flag;
		++i;
	}
	return flag;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
	size_t i = 0;
	int flag;
	while (i < len) {
		flag = *(str1 + i) - *(str2 + i);
		if (flag != 0)
			return flag;
		++i;
	}
	return flag;
}

size_t strlen(const char *str)
{
	size_t i = 0;

	for (; *str != '\0'; str++, i++)
		;

	return i;
}

char *strchr(const char *str, int c)
{
	size_t i;
	for (i = 0; *(str + i) != '\0'; ++i) {
		if (*(str + i) == c)
			return (char *)(str + i);
	}
	return NULL;
}

char *strrchr(const char *str, int c)
{
	size_t i;
	char *res = NULL;
	for (i = 0; *(str + i) != '\0'; ++i) {
		if (*(str + i) == c)
			res = (char *)str + i;
	}
	return res;
}

char *strstr(const char *haystack, const char *needle)
{
	size_t needle_len = strlen(needle);
	if (needle_len == 0)
		return (char *)haystack;

	size_t i, j;
	for (i = 0; *(haystack + i) != '\0'; ++i) {
		j = 0;
		while (*(haystack + i + j) != '\0' && *(haystack + i + j) == *(needle + j)) {
			++j;
		}

		if (j == needle_len)
			return (char *)(haystack + i);
	}
	return NULL;
}

char *strrstr(const char *haystack, const char *needle)
{
	size_t needle_len = strlen(needle);
	if (needle_len == 0)
		return (char *)haystack;

	char *res = NULL;
	size_t i, j;
	for (i = 0; *(haystack + i) != '\0'; ++i) {
		j = 0;
		while (*(haystack + i + j) != '\0' && *(haystack + i + j) == *(needle + j)) {
			++j;
		}

		if (j == needle_len)
			res = (char *)haystack + i;
	}
	return res;
}

char *strtok(char *str, const char *delimiters)
{
	static char *last;
	int i, j, ok = 0;
	
	if (!str)
		str = last;
	
	for (i = 0; *(str + i) != '\0' && !ok; ++i) {
		for (j = 0; *(delimiters + j) != '\0'; ++j) {
			if (*(str + i) == *(delimiters + j)) {
				*(str + i) = '\0';
				ok = 1;
				break;
			}
		}
        if (*(str + i + 1) == '\0')
            ok = 1;
	}

    if (ok) {
        last = str + i;
        return str;
    }

	return NULL;
}

void *memcpy(void *destination, const void *source, size_t num)
{
	char *dst = (char *)destination;
	char *src = (char *)source;
	size_t i;
	for (i = 0; i < num; ++i)
		*(dst + i) = *(src + i);
	return dst;
}

void *memmove(void *destination, const void *source, size_t num)
{
	char *dst = (char *)destination;
	const char *src = (char *)source;
	if (destination < source) {
		for (size_t i = 0; i < num; ++i)
			*(dst + i) = *(src + i);
	} else if (destination > source) {
		for (size_t i = num; i > 0; --i)
			*(dst + i - 1) = *(src + i - 1);
	}

	return destination;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	size_t i = 0;
	int flag;
	const char *str1 = (const char *)ptr1;
	const char *str2 = (const char *)ptr2;
	while (i < num) {
		flag = *(str1 + i) - *(str2 + i);
		if (flag != 0)
			return flag;
		++i;
	}
	return flag;
}

void *memset(void *source, int value, size_t num)
{
	char *src = (char *)source;
	size_t i;
	for (i = 0; i < num; ++i)
		*(src + i) = value;
	return source;
}

