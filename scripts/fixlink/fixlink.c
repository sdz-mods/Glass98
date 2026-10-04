/*****************************************************************************
 MIT No Attribution

 Copyright 2023-2024 Jaroslav Hensl <emulator@emulace.cz>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 IN THE SOFTWARE.
*****************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

const char help[] =
	"Usage: %s <mode> [--dry-run] exe_file_to_fix\n"
	"<mode> can be:\n"
	"-40: set expect Windows version to 4.0 (NE target)\n"
	"-vxd32: fix wrong paging and flags in wlink VXD (LE target)\n"
	"-shared: fix EXE/DLL to load to shared memory (PE target)\n"
	"-checksum: recalculate PE checksum (PE target)\n"
	"\n";

#pragma pack(push)
#pragma pack(1)

/* http://www.delorie.com/djgpp/doc/exe/ */
typedef struct EXE_header {
  uint16_t signature; /* == 0x5a4D */
  uint16_t bytes_in_last_block;
  uint16_t blocks_in_file;
  uint16_t num_relocs;
  uint16_t header_paragraphs;
  uint16_t min_extra_paragraphs;
  uint16_t max_extra_paragraphs;
  uint16_t ss;
  uint16_t sp;
  uint16_t checksum;
  uint16_t ip;
  uint16_t cs;
  uint16_t reloc_table_offset;
  uint16_t overlay_number;
	uint16_t e_res[4];
	uint16_t e_oemid;
	uint16_t e_oeminfo;
	uint16_t e_res2[10];
	uint32_t e_lfanew;
} EXE_header_t;

#define EXE_SIGN 0x5a4D

/* from: https://wiki.osdev.org/NE */
typedef struct NE_header
{
	uint16_t signature;          /*"NE", 0x4543 */
	uint8_t MajLinkerVersion;    /*The major linker version */
	uint8_t MinLinkerVersion;    /*The minor linker version */
	uint16_t EntryTableOffset;   /*Offset of entry table, see below */
	uint16_t EntryTableLength;   /*Length of entry table in bytes */
	uint32_t FileLoadCRC;        /*32-bit CRC of entire contents of file */
	uint8_t ProgFlags;           /*Program flags, bitmapped */
	uint8_t ApplFlags;           /*Application flags, bitmapped */
	uint16_t AutoDataSegIndex;   /*The automatic data segment index */
	uint16_t InitHeapSize;       /*The initial local heap size */
	uint16_t InitStackSize;      /*The initial stack size */
	uint32_t EntryPoint;         /*CS:IP entry point, CS is index into segment table */
	uint32_t InitStack;          /*SS:SP initial stack pointer, SS is index into segment table */
	uint16_t SegCount;           /*Number of segments in segment table */
	uint16_t ModRefs;            /*Number of module references (DLLs) */
	uint16_t NoResNamesTabSiz;   /*Size of non-resident names table, in bytes (Please clarify non-resident names table) */
	uint16_t SegTableOffset;     /*Offset of Segment table */
	uint16_t ResTableOffset;     /*Offset of resources table */
	uint16_t ResidNamTable;      /*Offset of resident names table */
	uint16_t ModRefTable;        /*Offset of module reference table */
	uint16_t ImportNameTable;    /*Offset of imported names table (array of counted strings, terminated with string of length 00h) */
	uint32_t OffStartNonResTab;  /*Offset from start of file to non-resident names table */
	uint16_t MovEntryCount;      /*Count of moveable entry point listed in entry table */
	uint16_t FileAlnSzShftCnt;   /*File alignment size shift count (0=9(default 512 byte pages)) */
	uint16_t nResTabEntries;     /*Number of resource table entries */
	uint8_t targOS;              /*Target OS */
	uint8_t OS2EXEFlags;         /*Other OS/2 flags */
	uint16_t retThunkOffset;     /*Offset to return thunks or start of gangload area - what is gangload? */
	uint16_t segrefthunksoff;    /*Offset to segment reference thunks or size of gangload area */
	uint16_t mincodeswap;        /*Minimum code swap area size */
	uint16_t expctwinver;        /*Expected windows version eg. 0x030A, 0x0400 */
} NE_header_t;

#define NE_SIGN 0x454E

/* more source, best is here:
	https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exeflat.h
 */
typedef struct LE_header
{
	uint16_t signature; /* "LX" 0x584C - 16bit or "LE" - 32bit */
	uint8_t  BOrd;
	uint8_t  WOrd;
	uint32_t Formatlevel;
	uint16_t CPUType; /* 0x08 */
	uint16_t OSType;
	uint32_t ModuleVersion;
	uint32_t ModulesFlags; /* 0x10 */
	uint32_t ModuleNumOfPages;
	uint32_t EIPObject; /* 0x18 */
	uint32_t EIP;
	uint32_t ESPObject; /* 0x20 */
	uint32_t ESP;
	uint32_t PageSize; /* 0x28 */
	uint32_t PageOffsetShift; /* 16 - offset, 32 - shift */
	uint32_t FixupSectionSize; /* 0x30 */
	uint32_t FixupSectionChecksum;
	uint32_t LoaderSectionSize; /* 0x38 */
	uint32_t LoaderSectionChecksum;
	uint32_t ObjectTableOff; /* 0x40 */
	uint32_t NumObjectsInModule;
	uint32_t ObjectPageTableOff; /* 0x48 */
	uint32_t ObjectInterPagesOff;
	uint32_t ResourceTableOffset; /* 0x50 */
	uint32_t ResourceTableEntries;
	uint32_t ResidentNameTLBOff; /* 0x58 */
	uint32_t EntryTableEntries;
	uint32_t ModulesDirectivesOff; /* 0x60 */
	uint32_t NumModuleDirectives;
	uint32_t FixupPagetableOff; /* 0x68 */
	uint32_t FixupRecordsTableOff;
	uint32_t ImportModuleTLBOff; /* 0x70 */
	uint32_t NumImportMODEntries;
	uint32_t ImportProcTlbOff; /* 0x78 */
	uint32_t PerPageChecksumOff;
	uint32_t DataPagesOffset; /* 0x80 */
	uint32_t NumPreloadPages;
	uint32_t NonResNameTLBOff; /* 0x88 */
	uint32_t NonResNameTLBLen;
	uint32_t NotResnameTLBChecksum; /* 0x90 */
	uint32_t AutoDSObject;
	uint32_t DebugInfoOff; /* 0x98 */
	uint32_t DebugInfoLength;
	uint32_t InstancePreload; /* 0xA0 */
	uint32_t InstanceDemand;
	uint32_t Heapsize; /* 0xA8 */
	uint32_t StackSize; /* OS2 only */
  union /* 0xB0 */
  {
		uint8_t Reserved[20]; /* pad to 196 bytes. */
    struct
    {
			uint8_t  ReservedVXD[8]; /* +0xB0 */
			uint32_t WinresOff;     /* +0xB8 Windows VxD version info resource offset */
			uint32_t WinresLen;     /* +0xBC Windows VxD version info resource lenght */
			uint16_t DeviceID;      /* +0xC0 Windows VxD device ID */
			uint16_t DDKversion;    /* +0xC2 Windows VxD DDK version (0x030A) */
		} vxd;
  };
} LE_header_t;

#define LE_SIGN 0x454C
#define LX_SIGN 0x584C

typedef struct LE_object
{
	uint32_t size;       /* object virtual size */
	uint32_t addr;       /* base virtual address */
	uint32_t flags;
	uint32_t mapidx;     /* page map index */
	uint32_t mapsize;    /* number of entries in page map */
	uint32_t reserved;
} LE_object_t;

#define OBJ_READABLE        0x0001
#define OBJ_WRITEABLE       0x0002
#define OBJ_EXECUTABLE      0x0004
#define OBJ_RESOURCE        0x0008
#define OBJ_DISCARDABLE     0x0010
#define OBJ_SHARABLE        0x0020
#define OBJ_HAS_PRELOAD     0x0040
#define OBJ_HAS_INVALID     0x0080
#define OBJ_PERM_SWAPPABLE  0x0100  /* LE */
#define OBJ_HAS_ZERO_FILL   0x0100  /* LX */
#define OBJ_PERM_RESIDENT   0x0200
#define OBJ_PERM_CONTIGUOUS 0x0300  /* LX */
#define OBJ_PERM_LOCKABLE   0x0400
#define OBJ_ALIAS_REQUIRED  0x1000
#define OBJ_BIG             0x2000
#define OBJ_CONFORMING      0x4000
#define OBJ_IOPL            0x8000

typedef struct LE_map_entry
{
	uint8_t page_num[3]; /* 24-bit page number in .exe file */
	uint8_t flags;
} LE_map_entry_t;

/*
 * PE Documentation here:
 *  https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
 *
 * PE checksum algorithm:
 *  https://bytepointer.com/resources/microsoft_pe_checksum_algo_distilled.htm
 *
 * Discussion about PE checksum (list of examples how don't do it):
 *  https://stackoverflow.com/questions/6429779/can-anyone-define-the-windows-pe-checksum-algorithm
 *
 */
typedef struct PE_signature
{
	uint16_t signature;
	uint16_t zero;
} PE_signature_t;

#define PE_SIGN 0x4550

typedef struct COFF_header
{
	uint16_t Machine;
	uint16_t NumberOfSections;
	uint32_t TimeDateStamp;
	uint32_t PointerToSymbolTable;
	uint32_t NumberOfSymbols;
	uint16_t SizeOfOptionalHeader;
	uint16_t Characteristics;
} COFF_header_t;

typedef struct PE_header
{
	/* Header Standard Fields */
	uint16_t Magic;
	uint8_t  MajorLinkerVersion;
	uint8_t  MinorLinkerVersion;
	uint32_t SizeOfCode;
	uint32_t SizeOfInitializedData;
	uint32_t SizeOfUninitializedData;
	uint32_t AddressOfEntryPoint;
	uint32_t BaseOfCode;
	uint32_t BaseOfData;
	/* Header Windows-Specific Fields */
	uint32_t ImageBase;
	uint32_t SectionAlignment;
	uint32_t FileAlignment;
	uint16_t MajorOperatingSystemVersion;
	uint16_t MinorOperatingSystemVersion;
	uint16_t MajorImageVersion;
	uint16_t MinorImageVersion;
	uint16_t MajorSubsystemVersion;
	uint16_t MinorSubsystemVersion;
	uint32_t Win32VersionValue;
	uint32_t SizeOfImage;
	uint32_t SizeOfHeaders;
	uint32_t CheckSum;
	uint16_t Subsystem;
	uint16_t DllCharacteristics;
	uint32_t SizeOfStackReserve;
	uint32_t SizeOfStackCommit;
	uint32_t SizeOfHeapReserve;
	uint32_t SizeOfHeapCommit;
	uint32_t LoaderFlags;
	uint32_t NumberOfRvaAndSizes;
} PE_header_t;

typedef struct PE_section
{
	uint8_t  Name[8];
	uint32_t VirtualSize;
	uint32_t VirtualAddress;
	uint32_t SizeOfRawData;
	uint32_t PointerToRawData;
	uint32_t PointerToRelocations;
	uint32_t PointerToLinenumbers;
	uint16_t NumberOfRelocations;
	uint16_t NumberOfLinenumbers;
	uint32_t Characteristics;
} PE_section_t;

#define PE32 0x10b
#define IMAGE_FILE_MACHINE_I386 0x14c
#define SIZE_OF_PE32 224

#define IMAGE_SCN_MEM_SHARED 0x10000000
#define IMAGE_SCN_MEM_DISCARDABLE 0x02000000

#pragma pack(pop)

/* error codes */
#define OK           0
#define ERROR_OPEN  -1
#define ERROR_READ -2
#define ERROR_WRITE -3
#define ERROR_SEEK  -4
#define ERROR_NOT_MZ  -5
#define ERROR_NOT_NE  -6
#define ERROR_NOT_LE  -7
#define ERROR_NOT_PE  -8
#define ERROR_LOW_BASE -9
#define ERROR_NOT_PE32  -10
#define ERROR_NOT_PE_I386 -11
#define ERROR_NO_FILE -12

typedef struct error_msg
{
	int code;
	const char *txt;
} error_msg_t;

error_msg_t error_msg_table[] = {
	{ERROR_OPEN,  "Cannot open file!"},
	{ERROR_READ,  "File read failed! (wrong or corrupted file)"},
	{ERROR_WRITE, "File write failed! (readonly/locked file or readonly medium)"},
	{ERROR_SEEK,  "Seek failed! (wrong or corrupted file)"},
	{ERROR_NOT_MZ, "Can't find MZ header! (DOS compatibility header corruped or missing)"},
	{ERROR_NOT_NE, "File is not New Executable (NE, *.drv)!"},
	{ERROR_NOT_LE, "File is not 32bit Linear Executable (LE, *.vxd)!"},
	{ERROR_NOT_PE, "File in not Portable Executable (PE, *.exe, *.dll)!"},
	{ERROR_LOW_BASE, "Image base must be >= 0x80000000 to load to shared space!"},
	{ERROR_NOT_PE32, "Wrong PE version, PE32 required!"},
	{ERROR_NOT_PE_I386, "EXE file architecture in not i386!"},
	{ERROR_NO_FILE, "No file specified"},
	{0, NULL}
};

long EXE_offset(FILE *f, EXE_header_t *outEXE)
{
	long offset = 0;
	if(fread(outEXE, sizeof(EXE_header_t), 1, f) == 1)
	{
		if(outEXE->signature == EXE_SIGN)
		{
			if(outEXE->e_lfanew == 0)
			{
				offset = outEXE->blocks_in_file * 512;
				if(outEXE->bytes_in_last_block)
				{
					offset -= (512 - outEXE->bytes_in_last_block);
				}
			}
			else
			{
				offset = outEXE->e_lfanew;
			}
		}
	}

	return offset;
}

bool read_header(FILE *f, size_t header_size, uint16_t magic, void *out)
{
	if(fread(out, header_size, 1, f) == 1)
	{
		if(*((uint16_t*)out) == magic)
		{
			return true;
		}
	}
	return false;
}

bool read_block(FILE *f, size_t block_size, void *out)
{
	if(fread(out, block_size, 1, f) == 1)
	{
		return true;
	}
	return false;
}

bool read_move(FILE *f, size_t offset_from_begin)
{
	return fseek(f, offset_from_begin, SEEK_SET) == 0;
}

bool read_block_begin(FILE *f, long offset, size_t block_size, void *out)
{
	if(read_move(f, offset))
	{
		return read_block(f, block_size, out);
	}
	return false;
}

bool writeback_block(FILE *f, size_t block_size, void *data)
{
	long offset = ftell(f);

	fseek(f, offset-block_size, SEEK_SET);
	if(fwrite(data, block_size, 1, f) == 1)
	{
		fseek(f, offset, SEEK_SET);
		return true;
	}
	fseek(f, offset, SEEK_SET);

	return false;
}

int fix_wlink_vxd(const char *file, bool dofix)
{
	EXE_header_t exe;
	LE_header_t  le;
	FILE *f;
	int rc = OK;
	long offset;

	f = fopen(file, "r+b");
	if(f != NULL)
	{
		offset = EXE_offset(f, &exe);
		if(offset > 0)
		{
			if(read_move(f, offset))
			{
				if(read_header(f, sizeof(LE_header_t), LE_SIGN, &le))
				{
					unsigned int i;
					for(i = 0; i < le.NumObjectsInModule; i++)
					{
						LE_object_t obj;
						uint32_t new_flags;
						if(read_block_begin(f,
							offset + le.ObjectTableOff + i*sizeof(LE_object_t),
							sizeof(LE_object_t), &obj))
						{
							printf("LE object #%d: (addr: %d, flags: %08X)", i, obj.addr, obj.flags);

							/* all VXD segments MUST be executable */
							new_flags = obj.flags | OBJ_EXECUTABLE;

							if(obj.addr != 0 || obj.flags != new_flags)
							{
								if(dofix)
								{
									obj.flags = new_flags;
									obj.addr = 0; /* VXD using flat model, so all pages must start from begining */

									if(!writeback_block(f, sizeof(LE_object_t), &obj))
									{
										rc = ERROR_WRITE;
										break;
									}

									printf(" -> (addr: %d, flags: %08X)", obj.addr, obj.flags);
								} /* dofix */
								else
								{
									printf(" != (addr: 0, flags: %08X)", new_flags);
								}
							} /* needfix */

							printf("\n");
						}
						else
						{
							rc = ERROR_READ;
							break;
						}
					} /* for */
				} else rc = ERROR_NOT_LE;
			} else rc = ERROR_READ;
		} else rc = ERROR_NOT_MZ;

		fclose(f);
	} else rc = ERROR_OPEN;

	return rc;
}

int fix_wlink_drv(const char *file, uint16_t new_expctwinver, bool dofix)
{
	EXE_header_t exe;
	NE_header_t   ne;
	FILE *f;
	int rc = OK;
	long offset;

	f = fopen(file, "r+b");
	if(f != NULL)
	{
		offset = EXE_offset(f, &exe);
		if(offset > 0)
		{
			if(read_move(f, offset))
			{
				if(read_header(f, sizeof(NE_header_t), NE_SIGN, &ne))
				{
					printf("NE.expctwinver = %04X", ne.expctwinver);
					if(ne.expctwinver != new_expctwinver)
					{
						ne.expctwinver = new_expctwinver;
						if(dofix)
						{
							if(!writeback_block(f, sizeof(NE_header_t), &ne))
							{
								rc = ERROR_WRITE;
							}
							printf(" -> %04X", ne.expctwinver);
						} /* dofix */
						else
						{
							printf(" != %04X", ne.expctwinver);
						}
					} /* need fix */
					printf("\n");
				} else rc = ERROR_NOT_NE;
			} else rc = ERROR_READ;
		} else rc = ERROR_NOT_MZ;

		fclose(f);
	} else rc = ERROR_OPEN;

	return rc;
}

int fix_pe_shared(const char *file, bool dofix)
{
	EXE_header_t exe;
	PE_signature_t pe_sign;
	COFF_header_t coff;
	PE_header_t pe;
	PE_section_t section;
	char section_name[9] = {0};

	FILE *f;
	int rc = OK;
	long offset;

	f = fopen(file, "r+b");
	if(f != NULL)
	{
		offset = EXE_offset(f, &exe);
		if(offset > 0)
		{
			if(read_move(f, offset))
			{
				if(read_header(f, sizeof(PE_signature_t), PE_SIGN, &pe_sign))
				{
					if(pe_sign.zero == 0)
					{
						if(read_block(f, sizeof(COFF_header_t), &coff))
						{
							if(coff.Machine == IMAGE_FILE_MACHINE_I386)
							{
								if(read_header(f, sizeof(PE_header_t), PE32, &pe))
								{
									printf("PE image base: %08X\n", pe.ImageBase);
									if(pe.ImageBase >= 0x80000000UL)
									{
										/* skip extra space which not in PE_header_t */
										if(fseek(f, SIZE_OF_PE32-sizeof(PE_header_t), SEEK_CUR) == 0)
										{
											unsigned int i;
											for(i = 0; i < coff.NumberOfSections; i++)
											{
												if(read_block(f, sizeof(PE_section_t), &section))
												{
													memcpy(section_name, section.Name, 8);
													printf("Section: %8s, flags = %08X", section_name, section.Characteristics);

													if(((section.Characteristics & IMAGE_SCN_MEM_SHARED) == 0) &&
														((section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE) == 0))
													{
														if(dofix)
														{
															section.Characteristics |= IMAGE_SCN_MEM_SHARED;
															if(!writeback_block(f, sizeof(PE_section_t), &section))
															{
																rc = ERROR_WRITE;
																break;
															}
															printf(" -> %08X", section.Characteristics);
														} /* dofix */
														else
														{
															printf(" != %08X", section.Characteristics | IMAGE_SCN_MEM_SHARED);
														}
													} /* need fix */
													printf("\n");

												} else rc = ERROR_READ;
											} /* for */
										} else rc = ERROR_READ;
									} else rc = ERROR_LOW_BASE;
								} else rc = ERROR_NOT_PE32;
							} else rc = ERROR_NOT_PE_I386;
						} else rc = ERROR_READ;
					} else rc = ERROR_NOT_PE;
				} else rc = ERROR_NOT_PE;
			} else rc = ERROR_READ;
		} else rc = ERROR_NOT_MZ;
	} else rc = ERROR_OPEN;

	return rc;
}

/* c89 - offsetof(PE_header_t, CheckSum) */
size_t offsetof_PE_header_CheckSum()
{
	PE_header_t *h = (void*)0;
	return (size_t)&(h->CheckSum);
}

int fix_pe_checksum(const char *file, bool dofix)
{
	EXE_header_t exe;
	PE_signature_t pe_sign;
	COFF_header_t coff;
	PE_header_t pe;

	FILE *f;
	int rc = OK;
	long offset;
	long offset_checksum;

	f = fopen(file, "r+b");
	if(f != NULL)
	{
		offset = EXE_offset(f, &exe);
		if(offset > 0)
		{
			if(read_move(f, offset))
			{
				if(read_header(f, sizeof(PE_signature_t), PE_SIGN, &pe_sign))
				{
					if(pe_sign.zero == 0)
					{
						if(read_block(f, sizeof(COFF_header_t), &coff))
						{
							if(coff.Machine == IMAGE_FILE_MACHINE_I386)
							{
								if(read_header(f, sizeof(PE_header_t), PE32, &pe))
								{
									uint32_t checksum = 0;
									long fpos = 0;
									int c;

									offset_checksum = ftell(f) + offsetof_PE_header_CheckSum() - sizeof(PE_header_t);
									fseek(f, 0, SEEK_SET);
									while(!feof(f))
									{
										uint16_t word = 0;

										/* first byte */
										if(fpos == offset_checksum) /* ignore original checksum */
										{
											fseek(f, 4, SEEK_CUR);
											fpos += 4;
										}
										c = fgetc(f);
										if(c == EOF)
										{
											break;
										}
										else
										{
											fpos++;
										}
										word = c;

										/* second byte */
										if(fpos == offset_checksum) /* ignore original checksum */
										{
											fseek(f, 4, SEEK_CUR);
											fpos += 4;
										}
										c = fgetc(f);
										if(c == EOF)
										{
											c = 0; /* when is file not word aligned, assume extra byte is 0 */
										}
										else
										{
											fpos++;
										}
										word |= c << 8; /* low endian */

										/* update checksum */
										checksum += word;
										checksum = (checksum >> 16) + (checksum & 0xffff);
									}
									checksum = (checksum >> 16) + (checksum & 0xffff);
									checksum += fpos; /* finaly sum file size with checksum */

									printf("PE checksum: %08X", pe.CheckSum);
									if(pe.CheckSum != checksum)
									{
										if(dofix)
										{
											fseek(f, offset_checksum, SEEK_SET);
											fwrite(&checksum, 4, 1, f);

											printf(" -> %08X", checksum);
										}
										else
										{
											printf(" != %08X", checksum);
										}
									} /* need fix */
									printf("\n");
								} else rc = ERROR_NOT_PE32;
							} else rc = ERROR_NOT_PE_I386;
						} else rc = ERROR_READ;
					} else rc = ERROR_NOT_PE;
				} else rc = ERROR_NOT_PE;
			} else rc = ERROR_READ;
		} else rc = ERROR_NOT_NE;
		fclose(f);
	} else rc = ERROR_OPEN;

	return rc;
}

#define MODE_UNSET 0
#define MODE_40 1
#define MODE_VXD32 2
#define MODE_SHARED 3
#define MODE_CHECKSUM 4

#define CMP(_s) (stricmp(argv[i], _s) == 0)

int main(int argc, char *argv[])
{
	const char *filename = NULL;
	bool dofix = true;
	int mode = MODE_UNSET;
	int i;
	int rc = OK;
	error_msg_t *err_msg;

	for(i = 1; i < argc; i++)
	{
		if(CMP("--dry-run"))
			dofix = false;
		else if(CMP("-40"))
			mode = MODE_40;
		else if(CMP("-vxd32"))
			mode = MODE_VXD32;
		else if(CMP("-shared"))
			mode = MODE_SHARED;
		else if(CMP("-checksum"))
			mode = MODE_CHECKSUM;
		else
			filename = argv[i];
	}

	if(mode != MODE_UNSET && filename == NULL)
	{
		rc = ERROR_NO_FILE;
	}
	else
	{
		switch(mode)
		{
			case MODE_40:
				rc = fix_wlink_drv(filename, 0x400, dofix);
				break;
			case MODE_VXD32:
				rc = fix_wlink_vxd(filename, dofix);
				break;
			case MODE_SHARED:
				rc = fix_pe_shared(filename, dofix);
				if(dofix && rc == OK)
				{
					rc = fix_pe_checksum(filename, true);
				}
				break;
			case MODE_CHECKSUM:
				rc = fix_pe_checksum(filename, dofix);
				break;
			default:
				printf(help, argv[0]);
				break;
		}
	}

	if(rc == OK)
		return EXIT_SUCCESS;

	for(err_msg = &error_msg_table[0]; err_msg->txt != NULL; err_msg++)
	{
		if(rc == err_msg->code)
		{
			fprintf(stderr, "Error: %s\n", err_msg->txt);
		}
	}

	return EXIT_FAILURE;
}
