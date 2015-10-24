#ifndef METADATA_H
#define METADATA_H

#include <iostream>

enum chmod_t : int { CH_READ = 2, CH_WRITE = 4, CH_ALL = 6, CH_NONE = 0 };
enum filetype_t : int { TYPE_DIRECTORY = 1, TYPE_FILE = 2, TYPE_ERROR = -1 };
struct MetaData
{
	MetaData()
	{
		//empty
	}
	MetaData(filetype_t type, std::string name, int location, int size, chmod_t rights)
	{
		mType = type;
		memset(pName, 0, 16);
		strcpy_s(pName, name.c_str());
		//name.copy(pName, sizeof(pName));
		mLocation = location;
		mSize = size;
		mRights = rights;
	}
	filetype_t mType;
	char pName[16];
	int mLocation;
	/* If type is directory, mSize (number of files and directories) * sizeof(MetaData) it contains */
	int mSize;
	chmod_t mRights;

};

#endif
