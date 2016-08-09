////////////////////////////////////////////////////////////////////////////////
//
// Test
//
// Version        : PELock v2.0
// Language       : C / Assembly
// Author         : Brad Miller (mudlord@mail.com)
// Web page       : https://www.mudlord.info
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

// compression algorithm name (for logging purposes)
const char szCompressionName[] = "CCX";
#define BLZ_HEADER_SIZE (3 * 4)



#if CHAR_BIT == 8
#  define octet(v) ((unsigned char) (v))
#else
#  define octet(v) ((v) & 0x00FF)
#endif

static void
write_be32(unsigned char *p, unsigned long val)
{
	p[0] = octet(val >> 24);
	p[1] = octet(val >> 16);
	p[2] = octet(val >> 8);
	p[3] = octet(val);
}

/*
* Read a 32-bit unsigned value in network order.
*/
static unsigned long
read_be32(const unsigned char *p)
{
	return ((unsigned long)octet(p[0]) << 24)
		| ((unsigned long)octet(p[1]) << 16)
		| ((unsigned long)octet(p[2]) << 8)
		| ((unsigned long)octet(p[3]));
}

//
// DWORD __stdcall Compress(const PBYTE lpInput, DWORD dwInput, PBYTE lpOutput, PDWORD lpdwOutput, LPCOMPRESSION_PROGRESS lpCompressionProgress, const char *lpszConfig, DWORD Reserved)
//
// compression routine
//
// [in]
// lpInput - data source to compress
// dwInput - input data size in bytes
// lpOutput - output buffer (already allocated by PELock, it's bigger than input data size, so the ratio can be negative)
// lpdwOutput - pointer to DWORD to receive output data (compressed) size
// lpCompressionProgress - callback routine
// lpszConfig - pointer to the current configuration file path (either pelock.ini or project file path)
// Reserved - for future use, set to 0
//
// [out]
// 0 for success, anything != 0 for an error
//


EXPORT DWORD __stdcall Compress(const PBYTE lpInput, DWORD dwInput, PBYTE lpOutput, PDWORD lpdwOutput, LPCOMPRESSION_PROGRESS lpCompressionProgress, const char *lpszConfig, DWORD Reserved)
{
	PBYTE lpIn = NULL, lpOut = NULL;
	DWORD i = 0;

	//
	// check input params (altrough it's almost impossible it will be invalid)
	//
	if ((lpInput == NULL) || (dwInput == 0) || (lpOutput == NULL))
	{
		return COMPESSION_ERROR_PARAM;
	}

	// helper variables
	lpIn = lpInput;
	lpOut = lpOutput;

	//
	// store decompressed buffer size (so that decompression routine knows that size)
	//
	*(DWORD *)lpOut = dwInput;
	lpOut += sizeof(DWORD);

	//
	// compression routine (in our case copy bytes from input to output buffer)
	//
	for (i = 0; i < dwInput; i++)
	{
		*lpOut++ = *lpIn++;

		// call callback procedure with input and output positions (offsets)
		// call it once per 1 kB
		if ((i % 1024) == 0)
		{
			lpCompressionProgress(i, i, 0);
		}
	}

	// 100% done
	lpCompressionProgress(i, i, 0);

	//
	// set compressed size (in our case it's the same as the input size + size of additional DWORD)
	//
	if (lpdwOutput != NULL)
	{
		*lpdwOutput = dwInput + sizeof(DWORD);
	}

	//
	// return 0 (success), anything != 0 is treated like an error
	//
	return COMPRESSION_OK;
}
//
// const char * __stdcall Name(DWORD Reserved)
//
// compression algorithm name
//
// [in]
// Reserved - for future use, set to 0
//
// [out]
// compression algorithm name (ansi)
//
EXPORT const char * __stdcall Name(DWORD Reserved)
{
	return szCompressionName;
}

//
// void __stdcall Configure(HWND hParent, const char *lpszConfig, DWORD Reserved)
//
// display configuration dialog / about box
//
// [in]
// hWndParent - handle to owner window (main PELock's window)
// lpszConfig - pointer to the current configuration file path (either pelock.ini or project file path)
// Reserved   - for future use, set to 0
//
EXPORT void __stdcall Configure(HWND hWndParent, const char *lpszConfig, DWORD Reserved)
{
	MessageBox(hWndParent, szCompressionName, lpszConfig, MB_ICONINFORMATION);
}

//
// entrypoint
//
BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		// initialize (allocate memory buffers etc.)
	case DLL_PROCESS_ATTACH:

		DisableThreadLibraryCalls(hModule);
		break;

		// cleanup
	case DLL_PROCESS_DETACH:

		break;
	}

	return TRUE;
}
