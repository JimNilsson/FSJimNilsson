#include "filesystem.h"

FileSystem::FileSystem() 
{
	mCurrentDir = "/";
	memset(pRAM, 0, 512);
	format();
}

std::string FileSystem::format()
{
	MetaData rootDir = MetaData(ENUM_DIRECTORY, "/", 0, sizeof(MetaData), CH_READ | CH_WRITE);
	memset(pRAM, 0, 512);
	pRAM[511] = -1;
	memcpy(pRAM, &rootDir, sizeof(MetaData));
	mMemblockDevice.writeBlock(0, pRAM);
	
	memset(pRAM, 0, 512);
	pRAM[511] = -1; //Last byte in block points to next block of file or dir if file or dir is more than one block big
	for (int i = 1; i < 249; ++i)
	{
		mMemblockDevice.writeBlock(i, pRAM);
	}
	memset(pRAM, 0, 512);
	pRAM[0] = 1;
	pRAM[249] = 1;
	mMemblockDevice.writeBlock(249, pRAM); //Last block is reserved for checking which blocks are free
	
	return std::string("Disk formatted.\n");
}

std::string FileSystem::ls()
{
	std::string retstr = "";
	unsigned int location = findLocation(0, mCurrentDir);
	mMemblockDevice.readBlock(location, pRAM);
	MetaData md;
	memcpy(&md, pRAM, sizeof(MetaData));
	int j = 0;
	int i = 0;
	while ((j + i) < md.mSize)
	{
		if (j + sizeof(MetaData) > 510)
		{
			int nextBlock = (int)pRAM[511];
			mMemblockDevice.readBlock(nextBlock, pRAM); //Last byte (unsigned char) points to next block if file is larger than 1 block
			i += j;
			j = 0;
			
		}
		MetaData temp;
		memcpy(&temp, &pRAM[j], sizeof(MetaData));
		std::string fp = mCurrentDir;
		fp.append(temp.pName);
		
		retstr += temp.pName;
		retstr += "      ";
		retstr += std::to_string(getSize(temp.mLocation, fp));
		retstr += "\n";
		j += sizeof(MetaData);
	}
	return retstr;
}

std::string FileSystem::ls(std::string & path)
{
	std::string retstr = "";
	unsigned int location = findLocation(0, path);
	if (location < 0)
		return std::string("Error in finding path.\n");
	mMemblockDevice.readBlock(location, pRAM);
	MetaData md;
	memcpy(&md, pRAM, sizeof(MetaData));
	int j = 0;
	int i = 0;
	while ((j + i) < md.mSize)
	{
		if (j * sizeof(MetaData) + sizeof(MetaData) > 510)
		{
			unsigned char nextBlock = (unsigned char)pRAM[511];
			mMemblockDevice.readBlock((int)nextBlock, pRAM); //Last byte (unsigned char) points to next block if file is larger than 1 block
			i += j * sizeof(MetaData) + sizeof(MetaData);
			j = 0;
		}
		MetaData temp;
		std::string fp = mCurrentDir;
		fp.append(temp.pName);
		memcpy(&temp, &pRAM[j], sizeof(MetaData));
		retstr += temp.pName;
		retstr += "      ";
		retstr += std::to_string(getSize(temp.mLocation, fp));
		retstr += "\n";
		j += sizeof(MetaData);

	}
	return retstr;
}

int FileSystem::findLocation(int blockNr, std::string& dirPath)
{
	std::vector<std::string> fnames = split(dirPath);
	MetaData md;
	mMemblockDevice.readBlock(blockNr, pRAM);
	memcpy(&md, pRAM, sizeof(MetaData));
	if (fnames.size() == 0) //if == 0, we are in "/",
	{
		return md.mLocation;
	}

	int maxSeek = md.mSize;
	int j = 1;
	while (fnames.back().compare(md.pName) != 0 && (j - 1) * sizeof(MetaData) <= maxSeek)
	{
		if (j * sizeof(MetaData) + sizeof(MetaData) > 510)
		{
			unsigned char nextBlock = (unsigned char)pRAM[511];
			if (nextBlock > 0)
			{
				mMemblockDevice.readBlock((int)nextBlock, pRAM); //Last byte (unsigned char) points to next block if dir is larger than 1 block
				j = 0;
			}
		}
		memcpy(&md, &pRAM[j*sizeof(MetaData)], sizeof(MetaData));
		++j;
		
	}
	//When loop quits, we either found it or it doesnt exist
	if (j * sizeof(MetaData) > maxSeek)
		return -1; //Error code for not found
	std::string newPath = dirPath.substr(fnames[0].length() + 1, dirPath.length() - fnames[0].length());
	if (md.mType == ENUM_DIRECTORY)
		return findLocation(md.mLocation, newPath);
	else
		return md.mLocation;

}

int FileSystem::getSize(int blockNr, std::string& path)
{
	return 0;
}

std::string FileSystem::create(std::string & filePath)
{
	if (filePath.compare(0, 1, "/") != 0)
		filePath = mCurrentDir + filePath;
	std::vector<std::string> psplit = split(filePath, '/');
	if (psplit[0].length() > 16)
		return std::string("Filenames larger than 16 characters are not supported\n");
	//Find directory where file will be created
	int curLoc = findLocation(0, filePath.substr(0, filePath.size() - psplit[0].size()));
	if (curLoc < 0)
		return std::string("Failed creating file, does the directory exist?\n");
	
	//Check if access to write in parent dir
	if (~(CH_WRITE & getMetaData(0, filePath.substr(0, filePath.size() - psplit[0].size())).mRights))
		return std::string("Access denied.\n");

	//Check if file already exists
	if (findLocation(0, filePath) >= 0)
		return std::string("File already exists.\n");

	//If directory exists and file !exists, create the file
	int newFileBlock = mMemblockDevice.findFreeBlock();
	if (newFileBlock < 0)
		return std::string("No free blocks availible\n");
	//Update the directory where the file will be created
	mMemblockDevice.readBlock(curLoc, pRAM);
	MetaData md;
	memcpy(&md, pRAM, sizeof(MetaData));
	//If the metadata cant fit in the directory, we need to expand the directory to another block
	if (md.mSize + sizeof(MetaData) > 510)
	{
		int newDirBlock = mMemblockDevice.findFreeBlock();
		if (newDirBlock < 0)
			return std::string("No free blocks availible.\n");
		//We need to allocate a new block for the directory
		pRAM[511] = (unsigned char)newDirBlock;
		md.mSize += sizeof(MetaData);
		memcpy(pRAM, &md, sizeof(MetaData));
		mMemblockDevice.writeBlock(curLoc, pRAM); //Update directory with new size

		mMemblockDevice.readBlock(249, pRAM); //Set new blocks as occupied
		pRAM[newDirBlock] = 1;
		pRAM[newFileBlock] = 1;
		mMemblockDevice.writeBlock(249, pRAM);

		memset(pRAM, 0, 512);
		MetaData newFile = MetaData(ENUM_FILE, psplit.front(), newFileBlock, 0, CH_ALL);
		memcpy(pRAM, &md, sizeof(MetaData));
		mMemblockDevice.writeBlock(newDirBlock, pRAM); //Write new block with dir info
	}
	else
	{
		md.mSize += sizeof(MetaData);
		memcpy(pRAM, &md, sizeof(MetaData));
		MetaData newFile = MetaData(ENUM_FILE, psplit.front(), newFileBlock, 0, CH_ALL);
		memcpy(&pRAM[md.mSize - sizeof(MetaData)], &newFile, sizeof(MetaData));
		mMemblockDevice.writeBlock(curLoc, pRAM);

		mMemblockDevice.readBlock(249, pRAM);
		pRAM[newFileBlock] = 1;
		mMemblockDevice.writeBlock(249, pRAM);
	}

	return std::string("File created at ").append(filePath).append("\n");
}

/* This function ended up really similar to create() */
std::string FileSystem::mkdir(std::string& filePath)
{
	//Check if relative or absolute path
	if (filePath.compare(0, 1, "/") != 0)
		filePath = mCurrentDir + filePath;
	std::vector<std::string> psplit = split(filePath, '/');
	if (psplit[0].length() > 16)
		return std::string("Filenames larger than 16 characters are not supported\n");

	//Find parent directory of directory to be created
	int curLoc = findLocation(0, filePath.substr(0, filePath.size() - psplit[0].size()));
	if (curLoc < 0)
		return std::string("Failed creating directy, does the parent directory exist?\n");

	//Check if access to write in parent directory
	if (~(CH_WRITE & getMetaData(0, filePath.substr(0, filePath.size() - psplit[0].size())).mRights))
		return std::string("Access denied.\n");

	//Check if directory already exists
	if (findLocation(0, filePath) >= 0)
		return std::string("Directory already exists.\n");

	//If parent directory exists and file !exists, create the file
	int newFileBlock = mMemblockDevice.findFreeBlock();
	if (newFileBlock < 0)
		return std::string("No free blocks availible\n");

	//Update the directory where the file will be created
	mMemblockDevice.readBlock(curLoc, pRAM);
	MetaData md;
	memcpy(&md, pRAM, sizeof(MetaData));
	//If the metadata cant fit in the directory, we need to expand the directory to another block
	if (md.mSize + sizeof(MetaData) > 510)
	{
		int newDirBlock = mMemblockDevice.findFreeBlock();
		if (newDirBlock < 0)
			return std::string("No free blocks availible.\n");
		//We need to allocate a new block for the directory
		pRAM[511] = (unsigned char)newDirBlock;
		md.mSize += sizeof(MetaData);
		memcpy(pRAM, &md, sizeof(MetaData));
		mMemblockDevice.writeBlock(curLoc, pRAM); //Update directory with new size

		mMemblockDevice.readBlock(249, pRAM); //Set new blocks as occupied
		pRAM[newDirBlock] = 1;
		pRAM[newFileBlock] = 1;
		mMemblockDevice.writeBlock(249, pRAM);

		memset(pRAM, 0, 512);
		MetaData newFile = MetaData(ENUM_DIRECTORY, psplit.front(), newFileBlock, sizeof(MetaData), CH_ALL);
		memcpy(pRAM, &md, sizeof(MetaData));
		mMemblockDevice.writeBlock(newDirBlock, pRAM); //Write new block with dir info
		
		memset(pRAM, 0, 512); //Write metadata into new directory block
		memcpy(pRAM, &md, sizeof(MetaData));
		mMemblockDevice.writeBlock(newFileBlock, pRAM);
	}
	else
	{
		md.mSize += sizeof(MetaData);
		memcpy(pRAM, &md, sizeof(MetaData));
		MetaData newFile = MetaData(ENUM_DIRECTORY, psplit.front(), newFileBlock, sizeof(MetaData), CH_ALL);
		memcpy(&pRAM[md.mSize - sizeof(MetaData)], &newFile, sizeof(MetaData));
		mMemblockDevice.writeBlock(curLoc, pRAM);

		//Write metadata into new directory block
		memset(pRAM, 0, 512);
		memcpy(pRAM, &newFile, sizeof(MetaData));
		mMemblockDevice.writeBlock(newFileBlock, pRAM);

		mMemblockDevice.readBlock(249, pRAM);
		pRAM[newFileBlock] = 1;
		mMemblockDevice.writeBlock(249, pRAM);
	}
	return std::string("New directory created at ").append(filePath).append("\n");
}

std::string FileSystem::cd(std::string & dir)
{
	//Check if we are already where we want to cd
	if (std::string("/").append(dir).append("/").compare(mCurrentDir) == 0)
		return std::string("");
	//If relative or absolute path
	if (dir.compare(0, 1, "/") != 0)
		dir = mCurrentDir + dir + "/";
	//Check if it exists
	if ( findLocation(0, dir) < 0)
		return std::string("Directory does not exist\n");
	//Make sure the user doesn't try to cd into a file
	MetaData temp = getMetaData(0, dir);
	if (temp.mType == ENUM_ERROR || temp.mType == ENUM_FILE)
		return dir.append(" is not a directory.\n");
	mCurrentDir = dir;
	return std::string("");
}

MetaData FileSystem::getMetaData(int blockNr, std::string & dirPath)
{
	std::vector<std::string> fnames = split(dirPath);
	MetaData md;
	mMemblockDevice.readBlock(blockNr, pRAM);
	memcpy(&md, pRAM, sizeof(MetaData));
	if (fnames.size() == 0) //if == 0, we are in "/",
	{
		return md;
	}

	int maxSeek = md.mSize;
	int j = 1;
	while (fnames.back().compare(md.pName) != 0 && (j - 1) * sizeof(MetaData) <= maxSeek)
	{
		if (j * sizeof(MetaData) + sizeof(MetaData) > 510)
		{
			unsigned char nextBlock = (unsigned char)pRAM[511];
			if (nextBlock > 0)
			{
				mMemblockDevice.readBlock((int)nextBlock, pRAM); //Last byte (unsigned char) points to next block if dir is larger than 1 block
				j = 0;
			}
		}
		memcpy(&md, &pRAM[j*sizeof(MetaData)], sizeof(MetaData));
		++j;

	}
	//When loop quits, we either found it or it doesnt exist
	if (j * sizeof(MetaData) > maxSeek)
		return MetaData(ENUM_ERROR,"",-1,-1,-1); //Error code for not found
	std::string newPath = dirPath.substr(fnames[0].length() + 1, dirPath.length() - fnames[0].length());
	if (md.mType == ENUM_DIRECTORY)
		return getMetaData(md.mLocation, newPath);
	else
		return md;
}

std::string FileSystem::rm(std::string & path)
{
	//Check if relative or absolute path
	if (path.compare(0, 1, "/") != 0)
		path = mCurrentDir + path;

	//Check if file/dir exists
	if (findLocation(0, path) < 0)
		return std::string("File does not exist\n");
	//Check if file is allowed to be removed
	if (~(getMetaData(0, path).mRights & CH_REMOVE))
		return std::string("Restricted access.\n");
	return std::string();
}

int FileSystem::remove(int blockNr, std::string & path)
{
	std::vector<std::string> fnames = split(path, '/');

	return 0;
}



std::string FileSystem::pwd()
{
	std::string ret = mCurrentDir;
	return ret.append("\n");
}