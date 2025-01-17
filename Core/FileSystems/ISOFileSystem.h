// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <map>
#include <string>

#include "FileSystem.h"

#include "BlockDevices.h"


class ISOFileSystem : public IFileSystem
{
	struct TreeEntry
	{
		TreeEntry(){}
		~TreeEntry()
		{
			for (unsigned int i=0; i<children.size(); i++)
				delete children[i];
			children.clear();
		}

		std::string name;
		u32 flags;
		u32 startingPosition;
		s64 size;
		bool isDirectory;
    bool isBlockSectorMode;  // "umd:" mode: all sizes and offsets are in 2048 byte chunks

		std::vector<TreeEntry*> children;
	};

	struct OpenFileEntry
	{
		TreeEntry *file;
		unsigned int seekPos;  // TODO: Make 64-bit?
		bool isRawSector;   // "/sce_lbn" mode
		u32 sectorStart;
		u32 openSize;
	};
	

	typedef std::map<u32,OpenFileEntry> EntryMap;
	EntryMap entries;
	IHandleAllocator *hAlloc;
	TreeEntry *treeroot;
	BlockDevice *blockDevice;

	TreeEntry entireISO;

	void ReadDirectory(u32 startsector, u32 dirsize, TreeEntry *root);
	TreeEntry *GetFromPath(std::string path);

public:
	ISOFileSystem(IHandleAllocator *_hAlloc, BlockDevice *_blockDevice);
	~ISOFileSystem();
	std::vector<PSPFileInfo> GetDirListing(std::string path);
	u32      OpenFile(std::string filename, FileAccess access);
	void     CloseFile(u32 handle);
	size_t   ReadFile(u32 handle, u8 *pointer, s64 size);
	size_t   WriteFile(u32 handle, const u8 *pointer, s64 size);
	size_t   SeekFile(u32 handle, s32 position, FileMove type);
	PSPFileInfo GetFileInfo(std::string filename);
	bool     OwnsHandle(u32 handle);

	virtual bool MkDir(const std::string &dirname) {return false;}
	virtual bool RmDir(const std::string &dirname) {return false;}
	virtual bool DeleteFile(const std::string &filename) {return false;}
};
