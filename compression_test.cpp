#include <cstdlib>
#include <vector>

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "CompressionLibrary/CompressionLibrary.h"
#ifdef WIN32
    #include <direct.h>
    #include <malloc.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

#define VER         "0.3"

int decomp(char * file, char* directory);
int comp(char * file, char* directory);
void std_err(void);
void io_err(void);



const int MOD_ADLER = 65521;

unsigned long adler32(unsigned char *data, size_t len) /* where data is the location of the data in physical memory and
											   len is the length of the data in bytes */
{
	unsigned long a = 1, b = 0;
	size_t index;
	/* Process each byte of the data in order */
	for (index = 0; index < len; ++index)
	{
		a = (a + data[index]) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}
	return (b << 16) | a;
}

BYTE *Load_Input_File(char *FileName, unsigned long *Size)
{
	BYTE *Memory;
	FILE *Input = fopen(FileName, "rb");

	if (!Input) return(NULL);
	// Get the filesize
	fseek(Input, 0, SEEK_END);
	*Size = ftell(Input);
	fseek(Input, 0, SEEK_SET);

	Memory = (BYTE *)malloc(*Size);
	if (!Memory) return(NULL);
	if (fread(Memory, 1, *Size, Input) != (size_t)*Size) return(NULL);
	if (Input) fclose(Input);
	Input = NULL;
	return(Memory);
}

DWORD _stdcall compress_progress(DWORD dwInputPosition, DWORD dwOutputPosition, DWORD Reserved)
{
	return 0;
}


//
// DWORD __stdcall Compress(const PBYTE lpInput, DWORD dwInput, PBYTE lpOutput, PDWORD lpdwOutput, 
// LPCOMPRESSION_PROGRESS lpCompressionProgress, const char *lpszConfig, DWORD Reserved)
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
typedef DWORD(WINAPI *PCOMPRESS)(const PBYTE lpInput, DWORD dwInput, PBYTE lpOutput, 
PDWORD lpdwOutput, LPCOMPRESSION_PROGRESS lpCompressionProgress, const char *lpszConfig, DWORD Reserved);
//
// DWORD __cdecl DecompressionProc(PDEPACK_INTERFACE lpDepackInterface, PVOID lpInputData, PVOID lpOutputData)
//
// decompression routine (raw code), this code will be binded to the main loader code
// of a PELock, so you must follow some rules:
//
// * all registers (except EAX) MUST BE preserved (use pushad and popad), flags can be destroyed
// * you must return decompressed size in EAX
// * this routine is __cdecl, you SHOULD NOT fix the stack with ret 12
// * you can use lpInputData as a temporary buffer
//
// [in]
// lpDepackInterface - pointer to the DEPACK_INTERFACE structure, it gives access to the
// WinApi functions, you can create very complex decompression procedures with it
// lpInputData - compressed data
// lpOutputData - where to put decompressed data
//
// [out]
// size of decompressed data
//
typedef DWORD (__cdecl *PDECOMPRESS)(PDEPACK_INTERFACE *lppDecompProc, PVOID lpInputData, PVOID lpOutputData);
//
// void __stdcall DecompressionProc(PVOID **lppDecompProc, DWORD *lpdwDecompProc, DWORD Reserved)
//
// returns pointer to the decompression routine & its size
//
// [in]
// Reserved - it should be 0
//
// [out]
// lppDecompProc - pointer to receive decompression routine address
// lpdwDecompProc - DWORD to receive decompression routine size
//
// [notes]
// this procedure is called only ONCE per session, so you can generate decompression code
// dynamically (eg. using polymorphic engine), then at DLL_PROCESS_DETACH you can release
// its memory
//
typedef void(WINAPI* PDECOMPRESSPROC)(PVOID *lppDecompProc, PDWORD lpdwDecompProc, DWORD Reserved);
typedef const char * (WINAPI* PNAME)(DWORD Reserved);
#define HEADER_SIZE (3 * 4)



int main(int argc, char *argv[]) {
	PCOMPRESS Compress;
	PDECOMPRESSPROC DecompressionProc;
	PNAME Name;
	PDECOMPRESS Decompress = NULL;
	
	PVOID DecompressFunc = NULL;
	DWORD DecompressorSize = 0;
	unsigned long compressed;
	unsigned long original_sz;
	unsigned long packed_sze;
	DEPACK_INTERFACE depackinterface = { 0 };
	depackinterface.lpLoadLibrary = LoadLibrary;
	depackinterface.lpGetProcAddress = GetProcAddress;
	depackinterface.lpVirtualAlloc = VirtualAlloc;
	depackinterface.lpVirtualFree = VirtualFree;

	BYTE *Original = Load_Input_File("unpacker.bin", &original_sz);
	unsigned long adler1 = adler32(Original, original_sz);
	
	HINSTANCE hDLL = LoadLibrary("CompressionLibrary.dll");
	if (hDLL != NULL)
	{
		Compress = (PCOMPRESS)GetProcAddress(hDLL,"Compress");
		DecompressionProc = (PDECOMPRESSPROC)GetProcAddress(hDLL, "DecompressionProc");
		Name = (PNAME)GetProcAddress(hDLL, "Name");
		
	}
	DecompressionProc((PVOID*)&Decompress, &DecompressorSize, 0);
	printf("using decompressor library '%s'\n", Name(0));
	printf("decompressor size is %d bytes\n", DecompressorSize);

	BYTE* compressed_data = (BYTE*)malloc(original_sz * 2);
	DWORD success = Compress(Original, original_sz, compressed_data, &compressed, compress_progress, NULL, 0);
	if (success)
	{
		printf("Error in compression!\n");
		return 0;
	}
	FILE* packedfile = fopen("packed.bin", "wb");
	fwrite((BYTE*)compressed_data, 1, compressed, packedfile);
	fclose(packedfile);
	BYTE* packed_test = Load_Input_File("packed.bin", &packed_sze);

	BYTE* depacked_data = (BYTE*)malloc(compressed * 2 + (1024 * 32));
	printf("decompressing!\n");
	DWORD depacked_sze = Decompress((PDEPACK_INTERFACE*)&depackinterface, packed_test, depacked_data);

	unsigned long adler2 = adler32(depacked_data, depacked_sze);
	
	if (adler1 != adler2)
	{
		printf("Data does not match!\n");
		return 1;
	}
	printf("Data matches!\n");
	free(compressed_data);
	free(packed_test);
	free(Original);
	return 0;
}
	
