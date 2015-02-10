#ifndef _AJ_TARGET_H
#define _AJ_TARGET_H
/**
 * @file
 */
/******************************************************************************
 * Copyright (c) 2012-2014, AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <windows.h>
#include <assert.h>
#include <stdlib.h>

#if _MSC_VER >= 1600   /* MSVC 2010 or higher */
#include <stdint.h>
#else
typedef signed char int8_t;           /** 8-bit signed integer */
typedef unsigned char uint8_t;        /** 8-bit unsigned integer */
typedef signed short int16_t;         /** 16-bit signed integer */
typedef unsigned short uint16_t;      /** 16-bit unsigned integer */
typedef signed int int32_t;           /** 32-bit signed integer */
typedef unsigned int uint32_t;        /** 32-bit unsigned integer */
typedef signed long long int64_t;     /** 64-bit signed integer */
typedef unsigned long long uint64_t;  /** 64-bit unsigned integer */
#endif

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif


#define WORD_ALIGN(x) ((x & 0x3) ? ((x >> 2) + 1) << 2 : x)
#define HOST_IS_LITTLE_ENDIAN  TRUE
#define HOST_IS_BIG_ENDIAN     FALSE

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	#define AJ_Printf(fmat, ...) \
		do { printf(fmat, ## __VA_ARGS__); fflush(stdout); } while (0)
#else // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	#if _DEBUG
		#if AJ_PRINTF_LOG
			#define AJ_Printf(fmat, ...)																\
				do																						\
				{																						\
					static char buf[1024];																\
					sprintf(buf, fmat, ## __VA_ARGS__);													\
					OutputDebugStringA(buf);															\
																										\
					static wchar_t* logFilename = L"\\AJ_Printf_Log.txt";								\
					static auto folder = Windows::Storage::ApplicationData::Current->LocalFolder;		\
					static const wchar_t* path = folder->Path->Data();									\
					static wchar_t wcsLogFile[1024];													\
					static char mbsLogFile[1024];														\
																										\
					wcscpy(wcsLogFile, path);															\
					wcsncat(wcsLogFile, logFilename, 1024);												\
					wcstombs(mbsLogFile, wcsLogFile, 1024);												\
																										\
					FILE* pFile = fopen(mbsLogFile, "a");												\
					if (pFile)																			\
					{																					\
						fwrite(buf, sizeof(char), strlen(buf), pFile);									\
						fclose(pFile);																	\
					}																					\
				}																						\
				while (0)
		#else // AJ_PRINTF_LOG
			#define AJ_Printf(fmat, ...)							\
				do													\
				{													\
					static char buf[1024];							\
					sprintf(buf, fmat, ## __VA_ARGS__);				\
					OutputDebugStringA(buf);						\
				}													\
				while (0)
		#endif // AJ_PRINTF_LOG
	#else // _DEBUG
		#define AJ_Printf(fmat, ...)
	#endif // _DEBUG
#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

#ifndef NDEBUG

extern uint8_t dbgCONFIGUREME;
extern uint8_t dbgINIT;
extern uint8_t dbgNET;
extern uint8_t dbgTARGET_CRYPTO;
extern uint8_t dbgTARGET_NVRAM;
extern uint8_t dbgTARGET_UTIL;

#endif

#define AJ_ASSERT(x)  assert(x)

/*
 * AJ_Reboot() is a NOOP on this platform
 */
#define AJ_Reboot()

#define AJ_EXPORT  __declspec(dllexport)

#define AJ_CreateNewGUID AJ_RandBytes

#define AJ_GetDebugTime(x) AJ_ERR_RESOURCES

#define inline __inline

/*
 * Main method allows argc, argv
 */
#define MAIN_ALLOWS_ARGS

#endif
