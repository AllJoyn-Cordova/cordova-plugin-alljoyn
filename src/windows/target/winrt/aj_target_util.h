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
#pragma once

#include <stdlib.h>


#define MAX_STR_LENGTH 2048


#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP || WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)


#define STRUCT_COPY(name, attrib) _ ## name->attrib = name->attrib


#define NULLABLE_TYPE_COPY(type, name)										\
	type* _ ## name = NULL;													\
	if (name != nullptr)													\
	{																		\
		*_ ## name = name->Value;											\
	}	


#define PLSTOMBS(wcs, mbs)																				\
	char mbs[MAX_STR_LENGTH];																			\
	do																									\
	{																									\
		int strLen = WideCharToMultiByte(CP_UTF8, 0, wcs->Data(), wcs->Length(), mbs, 0, NULL, NULL);	\
		WideCharToMultiByte(CP_UTF8, 0, wcs->Data(), wcs->Length(), mbs, MAX_STR_LENGTH, NULL, NULL);	\
		mbs[strLen] = '\0';																				\
	}																									\
	while (0);	


#define PLSTODYNMBS(wcs, mbs)																			\
	char* mbs = NULL;																					\
	do																									\
	{																									\
		int strLen = WideCharToMultiByte(CP_UTF8, 0, wcs->Data(), wcs->Length(), mbs, 0, NULL, NULL);	\
		mbs = new char[strLen + 1];																		\
		WideCharToMultiByte(CP_UTF8, 0, wcs->Data(), wcs->Length(), mbs, strLen, NULL, NULL);			\
		mbs[strLen] = '\0';																				\
	}																									\
	while (0);	


#define WCSTOMBS(wcs, mbs)																				\
	char mbs[MAX_STR_LENGTH];																			\
	do																									\
	{																									\
		int strLen = WideCharToMultiByte(CP_UTF8, 0, wcs, wcslen(wcs), mbs, 0, NULL, NULL);				\
		WideCharToMultiByte(CP_UTF8, 0, wcs, wcslen(wcs), mbs, MAX_STR_LENGTH, NULL, NULL);				\
		mbs[strLen] = '\0';																				\
	}																									\
	while (0);		


#define MBSTOWCS(mbs, wcs)																				\
	wchar_t wcs[MAX_STR_LENGTH];																		\
	do																									\
	{																									\
		int strLen = MultiByteToWideChar(CP_UTF8, 0, mbs, strlen(mbs), wcs, 0);							\
		MultiByteToWideChar(CP_UTF8, 0, mbs, strlen(mbs), wcs, MAX_STR_LENGTH);							\
		wcs[strLen] = '\0';																				\
	}																									\
	while (0);	


#define MBSTOPLS(mbs, wcs)																				\
	String^ wcs;																						\
	do																									\
	{																									\
		wchar_t __ ## wcs[MAX_STR_LENGTH];																\
		int strLen = MultiByteToWideChar(CP_UTF8, 0, mbs, strlen(mbs), __ ## wcs, 0);					\
		MultiByteToWideChar(CP_UTF8, 0, mbs, strlen(mbs), __ ## wcs, MAX_STR_LENGTH);					\
		__ ## wcs[strLen] = '\0';																		\
		wcs = ref new String(__ ## wcs);																\
	}																									\
	while (0);	


#define SAFE_DEL(p)															\
	if (p)																	\
	{																		\
		delete p;															\
		p = NULL;															\
	}


#define SAFE_DEL_ARRAY(p)													\
	if (p)																	\
	{																		\
		delete[] p;															\
		p = NULL;															\
	}

#endif // (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP || WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP