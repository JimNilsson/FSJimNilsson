#include "filesystem.h"
#include <fstream>

FileSystem::FileSystem() 
{
	mCurrentDir = "/";
	memset(pRAM, 0, 512);
	format();
}

std::string FileSystem::format()
{
	MetaData rootDir = MetaData(ENUM_DIRECTORY, "/", 1, 0, (chmod_t)(CH_READ | CH_WRITE));
	memset(pRAM, 0, 512);
	memcpy(pRAM, &rootDir, sizeof(MetaData));
	mMemblockDevice.writeBlock(0, pRAM);
	
	memset(pRAM, 0, 512);
	for (int i = 1; i < 249; ++i)
	{
		mMemblockDevice.writeBlock(i, pRAM);
	}
	memset(pRAM, 0, 512);
	pRAM[0] = 1;
	pRAM[1] = 1;
	pRAM[249] = 1;
	mMemblockDevice.writeBlock(249, pRAM); //Last block is reserved for checking which blocks are free
	
	return std::string("Disk formatted.\n");
}

std::string FileSystem::ls()
{
	std::string retstr = "./\n";
	unsigned int location = findLocation(0, mCurrentDir);
	int seekLength = getMetaData(0, mCurrentDir).mSize;
	mMemblockDevice.readBlock(location, pRAM);
	MetaData md;
	memcpy(&md, pRAM, sizeof(MetaData));
	int j = 0;
	int i = 0;
	while ((j + i) < seekLength)
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

int FileSystem::findLocation(int blockNr, std::string& dirPath, int seekLength)
{
	MetaData md;
	mMemblockDevice.readBlock(blockNr, pRAM);
	memcpy(&md, pRAM, sizeof(MetaData));
	
	if (blockNr == 0)
		return findLocation(md.mLocation, dirPath, md.mSize);

	std::vector<std::string> fnames = split(dirPath);
	if (fnames.size() == 0) 
	{
		return blockNr;
	}

	int j = 1;
	while (fnames.back().compare(md.pName) != 0 && (j - 1) * sizeof(MetaData) <= seekLength)
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
	if (j * sizeof(MetaData) > seekLength)
		return -1; //Error code for not found
	std::string newPath = dirPath.substr(fnames.back().length() + 1, dirPath.length() - fnames.back().length());
	if (md.mType == ENUM_DIRECTORY)
		return findLocation(md.mLocation, newPath, md.mSize);
	else
		return md.mLocation;

}

int FileSystem::findParentLocation(int blockNr, std::string & path, int seekLength)
{
	std::vector<std::string> fnames = split(path);
	MetaData md;
	mMemblockDevice.readBlock(blockNr, pRAM);
	memcpy(&md, pRAM, sizeof(MetaData));
	
	if (fnames.size() == 1)
	{
		return blockNr;
	}

	if (blockNr == 0)
		return findParentLocation(md.mLocation, path, md.mSize);

	int j = 1;
	while (fnames.back().compare(md.pName) != 0 && (j - 1) * sizeof(MetaData) <= seekLength)
	{
		if (j * sizeof(MetaData) >= 512 - sizeof(MetaData))
		{
			unsigned char nextBlock = (unsigned char)pRAM[511];
			if (nextBlock > 0)
			{
				mMemblockDevice.readBlock((int)nextBlock, pRAM); //Last byte (unsigned char) points to next block if dir is larger than 1 block
				blockNr = nextBlock;
				seekLength -= j * sizeof(MetaData);
				j = 0;
			}
		}
		memcpy(&md, &pRAM[j*sizeof(MetaData)], sizeof(MetaData));
		++j;

	}
	//When loop quits, we either found it or it doesnt exist
	if (j * sizeof(MetaData) > seekLength)
		return -1; //Error code for not found
	std::string newPath = path.substr(fnames.back().length() + 1, path.length() - fnames.back().length());
	fnames = split(newPath);
	if (fnames.size() == 1)
		return blockNr;
	return findParentLocation(md.mLocation, newPath, md.mSize);
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
	//Find directory where file metadata will be created
	std::string dirpath = filePath.substr(0, filePath.size() - psplit[0].size());
	int curLoc = findLocation(0, dirpath);
	if (curLoc < 0)
		return std::string("Failed creating file, does the directory exist?\n");

	//Check if access to write in parent dir
	std::string tst = filePath.substr(0, filePath.size() - psplit[0].size());
	if (!(CH_WRITE & getMetaData(0, filePath.substr(0, filePath.size() - psplit[0].size())).mRights))
		return std::string("Access denied.\n");

	//Check if file already exists
	if (findLocation(0, filePath) >= 0)
		return std::string("File already exists or attempted to create inside a file and not a directory.\n");

	//Make sure we try to write the file in a directory and not a file
	if (getMetaData(0, dirpath).mType != ENUM_DIRECTORY)
	return std::string("Can't create a file in a file\n");

	//If directory exists and file !exists, create the file
	int newFileBlock = mMemblockDevice.findFreeBlock();
	if (newFileBlock < 0)
		return std::string("No free blocks availible\n");

	//Find block which parent resides in
	int parentBlock = findParentLocation(0, filePath);
	if (parentBlock < 0)
		return std::string("Failure in finding block.\n");

	int inBlockLocation = 0;
	mMemblockDevice.readBlock(parentBlock, pRAM);
	MetaData temp;
	memcpy(&temp, pRAM, sizeof(MetaData));
	while (temp.mLocation != curLoc)
	{
		inBlockLocation += sizeof(MetaData);
		memcpy(&temp, &pRAM[inBlockLocation], sizeof(MetaData));
	}

	//Update the directory where the file will be created
	temp.mSize += sizeof(MetaData);
	if (temp.mSize > 510 && mMemblockDevice.findFreeBlock() < 0)
		return std::string("No free blocks availible.\n");
	memcpy(&pRAM[inBlockLocation], &temp, sizeof(MetaData));
	mMemblockDevice.writeBlock(parentBlock, pRAM);

	//Mark the block where file will reside as occupied first
	mMemblockDevice.readBlock(249, pRAM);
	pRAM[newFileBlock] = 1;
	mMemblockDevice.writeBlock(249, pRAM);

	//MetaData to be written into directory
	MetaData filedata = MetaData(ENUM_FILE, psplit.front(), newFileBlock, 0, CH_ALL);

	//If the metadata cant fit in the directory, we need to expand the directory to another block
	if (temp.mSize >= 512 - sizeof(MetaData))
	{
		int oldBlock = parentBlock;
		parentBlock = mMemblockDevice.findFreeBlock();
		if (parentBlock < 0)
		{
			//If fail, unreserve the block where the file would have resided
			mMemblockDevice.readBlock(249, pRAM);
			pRAM[newFileBlock] = 0;
			mMemblockDevice.writeBlock(249, pRAM);
			return std::string("No free blocks.\n");
		}

		curLoc = parentBlock;

		mMemblockDevice.readBlock(oldBlock, pRAM);
		pRAM[511] = (unsigned char)parentBlock; //Last byte points to "continuation"-block
		mMemblockDevice.writeBlock(oldBlock, pRAM);

		memset(pRAM, 0, 512);
		memcpy(pRAM, &filedata, sizeof(MetaData));
	}
	else
	{
		mMemblockDevice.readBlock(curLoc, pRAM);
		memcpy(&pRAM[temp.mSize - sizeof(MetaData)], &filedata, sizeof(MetaData));
	}
	mMemblockDevice.writeBlock(curLoc, pRAM);
	return std::string("New file created at ").append(filePath).append("\n");
}

/* This function ended up really similar to create() */
std::string FileSystem::mkdir(std::string& filePath)
{
	if (filePath.compare(0, 1, "/") != 0)
		filePath = mCurrentDir + filePath;
	std::vector<std::string> psplit = split(filePath, '/');
	if (psplit[0].length() > 16)
		return std::string("Filenames larger than 16 characters are not supported\n");
	//Find block where file metadata will be created
	std::string dirpath = filePath.substr(0, filePath.size() - psplit[0].size());
	int curLoc = findLocation(0, dirpath);
	int oldLoc = curLoc;
	if (curLoc < 0)
		return std::string("Failed creating file, does the directory exist?\n");
	//The directory might span several blocks, find the last one
	mMemblockDevice.readBlock(curLoc, pRAM);
	while (pRAM[511] != 0)
	{
		curLoc = pRAM[511];
		mMemblockDevice.readBlock(pRAM[511], pRAM);
	}

	//Check if access to write in parent dir
	std::string tst = filePath.substr(0, filePath.size() - psplit[0].size());
	if (!(CH_WRITE & getMetaData(0, filePath.substr(0, filePath.size() - psplit[0].size())).mRights))
		return std::string("Access denied.\n");

	//Check if file already exists
	if (findLocation(0, filePath) >= 0)
		return std::string("File already exists or attempted to create inside a file and not a directory.\n");


	//If directory exists and file !exists, create the file
	int newFileBlock = mMemblockDevice.findFreeBlock();
	if (newFileBlock < 0)
		return std::string("No free blocks availible\n");

	//Find block which parent resides in
	int parentBlock = findParentLocation(0, filePath);
	if (parentBlock < 0)
		return std::string("Failure in finding block.\n");


	int inBlockLocation = 0;
	mMemblockDevice.readBlock(parentBlock, pRAM);
	MetaData temp;
	memcpy(&temp, pRAM, sizeof(MetaData));
	while (temp.mLocation != oldLoc)
	{
		inBlockLocation += sizeof(MetaData);
		memcpy(&temp, &pRAM[inBlockLocation], sizeof(MetaData));
	}

	//Update the directory where the file will be created
	temp.mSize += sizeof(MetaData);
	if (temp.mSize % 512 >= 512 - sizeof(MetaData) && mMemblockDevice.findFreeBlock() < 0)
		return std::string("No free blocks availible.\n");
	memcpy(&pRAM[inBlockLocation], &temp, sizeof(MetaData));
	mMemblockDevice.writeBlock(parentBlock, pRAM);

	//Mark the block where file will reside as occupied first
	mMemblockDevice.readBlock(249, pRAM);
	pRAM[newFileBlock] = 1;
	mMemblockDevice.writeBlock(249, pRAM);

	//MetaData to be written into directory
	MetaData filedata = MetaData(ENUM_DIRECTORY, psplit.front(), newFileBlock, 0, CH_ALL);

	//If the metadata cant fit in the directory, we need to expand the directory to another block
	if (temp.mSize % 512 >= 512 - sizeof(MetaData))
	{
		int oldBlock = parentBlock;
		parentBlock = mMemblockDevice.findFreeBlock();
		if (parentBlock < 0)
		{
			//If fail, unreserve the block where the file would have resided
			mMemblockDevice.readBlock(249, pRAM);
			pRAM[newFileBlock] = 0;
			mMemblockDevice.writeBlock(249, pRAM);
			return std::string("No free blocks.\n");
		}

		mMemblockDevice.readBlock(curLoc, pRAM);
		pRAM[511] = (unsigned char)parentBlock; //Last byte points to "continuation"-block
		mMemblockDevice.writeBlock(curLoc, pRAM);

		curLoc = parentBlock;

		memset(pRAM, 0, 512);
		memcpy(pRAM, &filedata, sizeof(MetaData));
	}
	else
	{
		mMemblockDevice.readBlock(curLoc, pRAM);
		memcpy(&pRAM[(temp.mSize - 32) % 512], &filedata, sizeof(MetaData));
	}
	mMemblockDevice.writeBlock(curLoc, pRAM);
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

MetaData FileSystem::getMetaData(int blockNr, std::string & path, int seekLength)
{
	std::vector<std::string> fnames = split(path);
	MetaData md;
	mMemblockDevice.readBlock(blockNr, pRAM);
	memcpy(&md, pRAM, sizeof(MetaData));

	if (fnames.size() == 0)
	{
		return md;
	}

	if (blockNr == 0)
		return getMetaData(md.mLocation, path, md.mSize);

	int j = 1;
	while (fnames.back().compare(md.pName) != 0 && (j - 1) * sizeof(MetaData) <= seekLength)
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
	if (j * sizeof(MetaData) > seekLength)
		return MetaData(ENUM_ERROR, "", -1, -1, CH_ALL); //Error code for not found
	std::string newPath = path.substr(fnames.back().length() + 1, path.length() - fnames.back().length());
	fnames = split(newPath);
	if (fnames.size() == 0)
		return md;

	return getMetaData(md.mLocation, newPath, md.mSize);
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

void FileSystem::dumpHarddrive()
{
	FILE* f;
	fopen_s(&f, "datadump.txt", "w");
	std::string s;
	for (int i = 0; i < 250; ++i)
	{
		s = "";
		mMemblockDevice.readBlock(i, pRAM);
		MetaData md;
		s.append(std::to_string(i));
		s.append("   ");
		for (int j = 0; j < 15; ++j)
		{
			memcpy(&md, &pRAM[j*sizeof(MetaData)], sizeof(MetaData));
			s.append(std::to_string(md.mType));
			s.append(" ");
			s.append(md.pName);
			s.append(" ");
			s.append(std::to_string(md.mLocation));
			s.append(" ");
			s.append(std::to_string(md.mSize));
			s.append(" ");
			s.append(std::to_string(md.mRights));
			s.append(" | ");
		}
		s.append("nextblock: ");
		int nb = (unsigned char)pRAM[511];
		s.append(std::to_string(nb));
		s.append("\n");
		fwrite(s.c_str(), sizeof(char), s.size(), f);
		
	}
	fclose(f);


}