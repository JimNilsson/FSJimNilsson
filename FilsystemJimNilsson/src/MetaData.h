#ifndef METADATA_H
#define METADATA_H

#include <iostream>

enum chmod_t : int { CH_READ = 2, CH_WRITE = 4, CH_REMOVE = 1, CH_ALL = 7 };
enum filetype_t : int { ENUM_DIRECTORY = 1, ENUM_FILE = 2, ENUM_ERROR = -1 };
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
