#include "filesystem.h"
#include <fstream>
                                                                                                                                                                                                                                 /*Hidden comment: This file is written by J i m N i l s s o n. Since I host this publically at github, someone else taking this course might try to copy this code and pass it off as his own. This comment is here to make sure I don't get into trouble if someone tries to p l a g i a r i z e my work. */
FileSystem::FileSystem() 
{
	mCurrentDir = "/";
	memset(pRAM, 0, 512);
	format();
}

std::string FileSystem::format()
{
	MetaData rootDir = MetaData(ENUM_DIRECTORY, "/", 1, 0, (chmod_t)(CH_ALL));
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
	
	mCurrentDir = "/";
	return std::string("Disk formatted.\n");
}

std::string FileSystem::ls()
{
	return ls(mCurrentDir);
}

std::string FileSystem::ls(std::string path)
{
	path = pathToAbsolutePath(path);
	std::string retstr = "./\n";
	if (path.compare("/") != 0)
		retstr.append("../\n");
	std::string spaces("                    ");
	int location = findLocation(0, path);
	if (location < 0)
		return std::string("Invalid path.\n");
	int seekLength = getMetaData(0, path).mSize;
	mMemblockDevice.readBlock(location, pRAM);
	MetaData md;
	memcpy(&md, pRAM, sizeof(MetaData));
	int j = 0;
	int i = 0;
	while ((j + i) < seekLength)
	{
		if (j + sizeof(MetaData) > 510)
		{
			location = (unsigned char)pRAM[511]; //Last byte (unsigned char) points to next block if file is larger than 1 block
			i += j;
			j = 0;

		}
		mMemblockDevice.readBlock(location, pRAM);
		MetaData temp;
		memcpy(&temp, &pRAM[j], sizeof(MetaData));
		std::string fp = path;
		fp.append(temp.pName).append("/");

		std::string fsize = std::to_string(getSize(fp)).append(" B");
		std::string rights = rightsToText(temp.mRights);
		std::string ftype = typeToText(temp.mType);

		retstr += temp.pName;
		retstr += spaces.substr(0, spaces.size() - strlen(temp.pName));
		retstr += fsize;
		retstr += spaces.substr(0, spaces.size() - fsize.size() - 12);
		retstr += rights;
		retstr += spaces.substr(0, spaces.size() - rights.size() - 12);
		retstr += ftype;
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
	if (fnames.size() == 0)
		return 0;
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

int FileSystem::getSize(std::string path)
{
	path = pathToAbsolutePath(path);
	MetaData md = getMetaData(0, path);
	
	//If it's a file, end of recursion.
	if (md.mType == ENUM_FILE)
		return md.mSize;

	std::string otherDir("");
	int size = 0;
	int dirPos = 0;
	int startLoc = md.mLocation;
	MetaData temp;
	while (dirPos < md.mSize)
	{
		mMemblockDevice.readBlock(startLoc, pRAM);
		memcpy(&temp, &pRAM[dirPos % (512 - sizeof(MetaData))], sizeof(MetaData));
		otherDir = path + temp.pName;
		size += getSize(otherDir);
		dirPos += sizeof(MetaData);
		if (dirPos % (512 - sizeof(MetaData)) == 0 && dirPos != 0)
			startLoc = (unsigned char)pRAM[511];
	}
	return size;
}

std::string FileSystem::create(std::string filePath, filetype_t type)
{
	filePath = pathToAbsolutePath(filePath);
	std::vector<std::string> psplit = split(filePath, '/');
	if (psplit[0].length() > 15)
		return std::string("Filenames larger than 16 characters are not supported\n");
	//Find block where file metadata will be created
	std::string dirpath = trimLastSectionOfPath(filePath);
	int curLoc = findLocation(0, dirpath);
	int oldLoc = curLoc;
	if (curLoc < 0)
		return std::string("Failed creating file, does the directory exist?\n");
	//The directory might span several blocks, find the last one
	mMemblockDevice.readBlock(curLoc, pRAM);
	while (pRAM[511] != 0)
	{
		curLoc = (unsigned char) pRAM[511];
		mMemblockDevice.readBlock(curLoc, pRAM);
	}

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
	MetaData filedata = MetaData(type, psplit.front(), newFileBlock, 0, CH_ALL);

	//If the metadata cant fit in the directory, we need to expand the directory to another block
	if (((temp.mSize - 32) / 32) % 15 == 0 && temp.mSize != 32)
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
	//	std::cout << "Metadata cool written at block: " << curLoc << " Location " << 0 << "\n";
	}
	else
	{
	//	std::cout << "Metadata written at block: " << curLoc << " Location " << (temp.mSize - 32) % (512 -32) << "\n";
		mMemblockDevice.readBlock(curLoc, pRAM);
		memcpy(&pRAM[(temp.mSize - 32) % (512-32)], &filedata, sizeof(MetaData));
	}
	
	mMemblockDevice.writeBlock(curLoc, pRAM);
	if (type == ENUM_FILE)
	{
		return std::string("New file created at ").append(filePath).append("\n");
	}
	else
		return std::string("New directory created at ").append(filePath).append("\n");
}


std::string FileSystem::cd(std::string  dir)
{
	
	//If relative or absolute path
	dir = pathToAbsolutePath(dir);
	//Check if it exists
	if ( findLocation(0, dir) < 0)
		return std::string("Directory does not exist\n");
	//Make sure the user doesn't try to cd into a file
	MetaData temp = getMetaData(0, dir);
	if (temp.mType == ENUM_ERROR || temp.mType == ENUM_FILE)
		return dir.append(" is not a directory.\n");
	mCurrentDir = dir;
	//If there's no "/" at the end of mCurrentDir, add it for consistency
	if (mCurrentDir.back() != '/')
		mCurrentDir.append("/");
	return std::string("");
}

std::string FileSystem::chmod(chmod_t permission, std::string filepath)
{
	if (permission != CH_READ && permission != CH_WRITE && permission != CH_ALL && permission != CH_NONE)
		return std::string("Invalid permission. Permitted values are 0 = NONE, 2 = READ, 4 = WRITE, 6 = READ/WRITE.\n");
	filepath = pathToAbsolutePath(filepath);
	int mdlocation = findParentLocation(0, filepath);
	if (mdlocation < 0)
		return std::string("File not found.\n");
	MetaData md = getMetaData(0, filepath);
	if (md.mType != ENUM_FILE)
		return std::string("Permissions are only for files, not directories.\n");
	md.mRights = permission;
	if (changeMetaData(filepath, md) < 0)
		return std::string("Failed when trying to change permissions.\n");
	return std::string("Permission changed.\n");
}

std::string FileSystem::rename(std::string source, std::string newName)
{
	source = pathToAbsolutePath(source);
	if (findLocation(0, source) < 0)
		return std::string("File not found.\n");
	if(newName.find('/') < newName.npos)
		return std::string("Illegal name.\n");
	if (newName.size() > 15)
		return std::string("File names longer than 16 characters are not allowed.\n");
	//Make sure the new name isnt already a name of a file in same directory
	std::string newPath = pathToAbsolutePath(trimLastSectionOfPath(source).append(newName));
	if (findLocation(0, newPath) >= 0)
		return std::string("A file with that name already exists.\n");
	MetaData md = getMetaData(0, source);
	strcpy_s(md.pName, newName.c_str());
	if (changeMetaData(source, md) < 0)
		return std::string("Could not rename file.\n");
	return std::string("File renamed.\n");
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

int FileSystem::findMetaDataLocation(int blockNr, std::string path, int seekLength)
{
	std::vector<std::string> fnames = split(path);
	MetaData md;
	mMemblockDevice.readBlock(blockNr, pRAM);
	memcpy(&md, pRAM, sizeof(MetaData));

	if (fnames.size() == 0)
	{
		return blockNr;
	}

	if (blockNr == 0)
		return findMetaDataLocation(md.mLocation, path, md.mSize);

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
				blockNr = nextBlock;
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
	if (fnames.size() == 0)
		return blockNr;

	return findMetaDataLocation(md.mLocation, newPath, md.mSize);
}

std::string FileSystem::rm(std::string & path)
{
	//Check if relative or absolute path
	path = pathToAbsolutePath(path);

	//If the user tries to remove root, we'll just assume they want to format the drive.
	if (path.compare("/") == 0)
	{
		format();
		return std::string("Everthing has been removed. Disk formatted.\n");
	}

	int fileLoc = findLocation(0, path);
	MetaData fileMD = getMetaData(0, path);
	//Check if file/dir exists
	if (fileLoc < 0)
		return std::string("File does not exist\n");
	//Check if file is allowed to be removed, we'll use the write flag for this
	if (!(fileMD.mRights & CH_WRITE))
		return std::string("Access denied.\n");
	//Only files and empty directories are allowed to be removed
	if (fileMD.mType == ENUM_DIRECTORY && fileMD.mSize != 0)
		return std::string("Only files and empty directories can be removed.\n");

	//Mark the blocks as free again. Also fill them with zeroes for secure deletion.
	int loc = fileLoc;
	while (loc != 0)
	{
		mMemblockDevice.readBlock(249, pRAM);
		pRAM[loc] = 0;
		mMemblockDevice.writeBlock(249, pRAM);
		mMemblockDevice.readBlock(loc, pRAM);
		int temploc = (unsigned char)pRAM[511];
		memset(pRAM, 0, 512);
		mMemblockDevice.writeBlock(loc, pRAM);
		loc = temploc;
		
	}

	//Remove the file's metadata and internally rearrange the block to fill the empty space
	int mdLocation = findMetaDataLocation(0, path); //The block where the metadata of the file is kept
	int inBlockLocation = findMetaDataInBlock(fileMD, mdLocation); //Now to find where in the block it is
	char tempArr[512]; //we need another 512 bytes of "memory" for this.
	int nextLoc = 0; //Directories might span several blocks, need this to keep track of where to read next
	do
	{
		mMemblockDevice.readBlock(mdLocation, pRAM);
		for (int i = inBlockLocation; i < 512 - (2 * sizeof(MetaData)); ++i)
		{
			pRAM[i] = pRAM[i + sizeof(MetaData)];
		}
		nextLoc = (unsigned char) pRAM[511];
		if (nextLoc != 0)
		{
			mMemblockDevice.readBlock(nextLoc, tempArr);
			memcpy(&pRAM[512 - (2*sizeof(MetaData))], tempArr, sizeof(MetaData));
		}
		mMemblockDevice.writeBlock(mdLocation,pRAM);
		inBlockLocation = 0;
		mdLocation = nextLoc;
	} while (nextLoc != 0);

	//Now we need to update the metadata of the directory that kept the file
	std::string parentPath = trimLastSectionOfPath(path);
	MetaData md = getMetaData(0, parentPath);
	md.mSize -= sizeof(MetaData);
	changeMetaData(parentPath, md);

	return std::string("File has been deleted.\n");
}

/* returns the position of the metadata in a block*/
int FileSystem::findMetaDataInBlock(const MetaData & md, int blockNr)
{
	int inBlockLocation = 0;
	MetaData comp;
	mMemblockDevice.readBlock(blockNr, pRAM);
	memcpy(&comp, pRAM, sizeof(MetaData));
	while (md.mLocation != comp.mLocation && inBlockLocation < 512)
	{
		inBlockLocation += 32;
		memcpy(&comp, &pRAM[inBlockLocation], sizeof(MetaData));
	}
	if (inBlockLocation >= 512)
		return -1;
	return inBlockLocation;
}


/* Appends string content to end of file pointed to by filePath */
int FileSystem::appendString(std::string path, std::string content)
{
	//Check if relative or absolute path
	path = pathToAbsolutePath(path);

	//Find block where content will be written
	int blockToWrite = findLocation(0, path);
	if (blockToWrite < 0)
		return -1;
	//File might span several blocks
	mMemblockDevice.readBlock(blockToWrite, pRAM);
	while (pRAM[511] != 0)
	{
		blockToWrite = (unsigned char) pRAM[511];
		mMemblockDevice.readBlock(blockToWrite, pRAM);
	}

	//Check if there is enough space to write the content and if we have write access
	MetaData md = getMetaData(0, path);
	if (!(md.mRights & CH_WRITE))
		return -1;
	int freeSpace = mMemblockDevice.spaceLeft() * 511; //Only 511 usable bytes per block due to last one being reserved for pointer to next block if there is one
	if (content.size() > (511 - (md.mSize % 511)) + freeSpace)
		return -1;

	//Write the data
	int initialWrite = std::min(511 - (md.mSize % 511), (int)content.size());
	int bytesWritten = 0;
	if (md.mSize == 0 || (initialWrite > 0 && md.mSize % 511 != 0))
	{
		mMemblockDevice.readBlock(blockToWrite, pRAM);
		memcpy(&pRAM[md.mSize % 511], content.c_str(), initialWrite);
		mMemblockDevice.writeBlock(blockToWrite, pRAM);
		bytesWritten = initialWrite;
	}
	while (bytesWritten < content.size())
	{
		//If there's more data to write we need to allocate a new block.		
		int newBlock = mMemblockDevice.findFreeBlock();
		if (newBlock < 0)
			return -1;
		//Mark new block as occupied
		mMemblockDevice.readBlock(249, pRAM);
		pRAM[newBlock] = 1;
		mMemblockDevice.writeBlock(249, pRAM);
		//Make previous block point to the new block at th end
		mMemblockDevice.readBlock(blockToWrite, pRAM);
		pRAM[511] = (unsigned char)newBlock; //Point to new block
		mMemblockDevice.writeBlock(blockToWrite, pRAM);

		//Write data to new block
		blockToWrite = newBlock;
		mMemblockDevice.readBlock(blockToWrite, pRAM);
		int toWrite = std::min(511, (int)(content.size() - bytesWritten));
		memcpy(pRAM, content.substr(bytesWritten, toWrite).c_str(), toWrite);
		mMemblockDevice.writeBlock(blockToWrite, pRAM);
		bytesWritten += toWrite;
	}

	//Update the metadata with the new size
	md.mSize += bytesWritten;
	//Find location of metadata to update
	int metaBlock = findMetaDataLocation(0, path);
	int inBlockLocation = findMetaDataInBlock(md,metaBlock);
	mMemblockDevice.readBlock(metaBlock, pRAM);
	memcpy(&pRAM[inBlockLocation], &md, sizeof(MetaData));
	mMemblockDevice.writeBlock(metaBlock, pRAM);

	return bytesWritten;
}


int FileSystem::changeMetaData(std::string filepath,const MetaData & md)
{
	filepath = pathToAbsolutePath(filepath);
	MetaData mdDst = getMetaData(0, filepath);
	if (mdDst.mType == ENUM_ERROR)
		return -1;
	int mdLoc = findMetaDataLocation(0, filepath);
	if (mdLoc < 0)
		return -1;
	if (filepath.compare("/") == 0) //Special case
		mdLoc = 0;
	
	int inBlockLocation = findMetaDataInBlock(mdDst,mdLoc);
	memcpy(&pRAM[inBlockLocation], &md, sizeof(MetaData));
	mMemblockDevice.writeBlock(mdLoc, pRAM);
	
	return 0;
}

std::string FileSystem::append(std::string sourcefile, std::string destfile)
{
	//Check if relative or absolute path
	sourcefile = pathToAbsolutePath(sourcefile);
	destfile = pathToAbsolutePath(destfile);

	//Make sure the files exist
	if (findLocation(0, sourcefile) < 0)
		return std::string("File missing.\n");
	if (findLocation(0, destfile) < 0)
		return std::string("File missing.\n");
	

	//Find metadata of files
	MetaData sourceMetaData = getMetaData(0, sourcefile);
	MetaData destMetaData = getMetaData(0, destfile);
	//Make sure they are files
	if (sourceMetaData.mType != ENUM_FILE || destMetaData.mType != ENUM_FILE)
		return std::string("append only works on files, not directories.\n");
	//Make sure we have permission
	if (!(sourceMetaData.mRights & CH_READ) || !(destMetaData.mRights & CH_WRITE))
		return std::string("Access denied.\n");
	//Make sure there's enough space on hard drive
	int fs = mMemblockDevice.spaceLeft() * 511;
	if (mMemblockDevice.spaceLeft() * 511 < sourceMetaData.mSize)
		return std::string("Not enough space left.\n");
	int blockToRead = sourceMetaData.mLocation;
	int bytesToWrite = sourceMetaData.mSize;
	int bytesWritten = 0;
	while (bytesWritten < bytesToWrite)
	{
		if (bytesWritten > 0)
		{
			mMemblockDevice.readBlock(blockToRead, pRAM);
			blockToRead = (unsigned char) pRAM[511];
		}

		std::string appstr = mMemblockDevice.readBlock(blockToRead).toString();
		appstr = appstr.substr(0, std::min(bytesToWrite - bytesWritten, 511));
		int k;
		k = appendString(destfile, appstr);
		if (k < 0)
			return std::string("Something went wrong when appending to file.\n");
		bytesWritten += k;
	}
	

	return std::string("Successfully appended ").append(sourcefile).append(" to ").append(destfile).append("\n");
}


std::string FileSystem::cat(std::string path)
{
	//Check if relative or absolute path
	path = pathToAbsolutePath(path);
	
	//Check if the file exists
	int fileLoc = findLocation(0, path);
	if (fileLoc < 0)
		return std::string("File not found.\n");

	//Check if it is a file
	MetaData md = getMetaData(0, path);
	if (md.mType != ENUM_FILE)
		return std::string(path).append(" is not a file.\n");
	if (!(md.mRights & CH_READ))
		return std::string("Access denied.\n");

	std::string retstring("");
	int byteCount = 0;
	while (byteCount < md.mSize)
	{
		if (fileLoc < 0)
			return std::string("cat failed, trying to access invalid block.\n");
		mMemblockDevice.readBlock(fileLoc, pRAM);
		int toCopy = std::min(511, md.mSize - byteCount);
		retstring.append(pRAM, toCopy);
		byteCount += toCopy;
		fileLoc = (unsigned char)pRAM[511];
	}
	return retstring;
}

std::string FileSystem::copy(std::string source, std::string dest)
{
	if (dest.size() == 0)
		return std::string("Missing name.\n");
	source = pathToAbsolutePath(source);
	dest = pathToAbsolutePath(dest);
	if (findLocation(0, dest) > 0)
		return std::string("File already exists.\n");
	MetaData sourceMD = getMetaData(0, source);
	if (!(sourceMD.mRights & CH_READ))
		return std::string("Access denied.\n");

	if (sourceMD.mType != ENUM_FILE)
		return std::string("Only files can be copied.\n");

	create(dest, ENUM_FILE);
	append(source, dest);
	return std::string("Copied ").append(source).append(" to ").append(dest).append("\n");
}

std::string FileSystem::pwd()
{
	std::string ret = mCurrentDir;
	return ret.append("\n");
}

std::string FileSystem::pathToAbsolutePath(std::string path)
{
	if (path.back() != '/')
		path.append("/");
	if (path.compare(0, 1, "/") == 0)
		return path;
	if (path.compare(0, 2, "./") == 0)
		return mCurrentDir + path.substr(2,path.size());
	if (path.compare(0, 1, ".") != 0)
		return mCurrentDir + path;
	
	/* If the user tries to ../ too far, it will just go to root directory. */
	int goParentCount = 0;
	while (path.compare(0, 3, "../") == 0)
	{
		++goParentCount;
		path = path.substr(3, path.size());
	}
	std::vector<std::string> ps = split(mCurrentDir, '/');
	std::reverse(ps.begin(), ps.end());
	std::string retpath("/");
	for (int i = 0; i < goParentCount && ps.size() != 0; ++i)
		ps.pop_back();
	std::reverse(ps.begin(), ps.end());
	for (int j = 0; j < ps.size(); ++j)
	{
		retpath.append(ps.back()).append("/");
		ps.pop_back();
	}
	retpath.append(path);
	return retpath;
}

std::string FileSystem::save(const std::string & saveFile) 
{
	std::ofstream out;
	out.open(saveFile.c_str(), std::ios::out | std::ios::trunc | std::ofstream::binary);
	if (!out.is_open())
		return std::string("Failed to open file.\n");
	for (int i = 0; i < 250; ++i)
	{
		
		mMemblockDevice.readBlock(i, pRAM);
		out.write(pRAM, 512);
	}
	out.close();
	
	return std::string("Saved image of harddrive to ").append(saveFile).append("\n");
}

std::string FileSystem::read(const std::string & saveFile)
{
	std::ifstream in;
	in.open(saveFile.c_str(), std::ios::in | std::ifstream::binary);
	if (!in.is_open())
		return std::string("Failed to open file.\n");
	for (int i = 0; i < 250; ++i)
	{
		in.read(pRAM, 512);
		mMemblockDevice.writeBlock(i, pRAM);
	}
	in.close();
	mCurrentDir = "/";
	return std::string("Hard drive image loaded.\n");
}

/*Used for debugging purposes*/
void FileSystem::dumpHarddrive()
{
	FILE* f;
	FILE* d;
	fopen_s(&f, "datadump.txt", "w");//Shows metadata
	fopen_s(&d, "rawdatadump.txt", "w");//just raw data
	std::string s;
	for (int i = 0; i < 250; ++i)
	{
		s = "";
		mMemblockDevice.readBlock(i, pRAM);
		fwrite(pRAM, sizeof(char), 511, d);
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
			s.append(rightsToText(md.mRights));
			s.append(" | ");
		}
		s.append("nextblock: ");
		int nb = (unsigned char)pRAM[511];
		s.append(std::to_string(nb));
		s.append("\n");
		fwrite(s.c_str(), sizeof(char), s.size(), f);
		
	}
	fclose(d);
	fclose(f);
}

std::string FileSystem::rightsToText(chmod_t rights)
{
	std::string retstr("");
	if (rights & CH_READ)
		retstr.append("R");
	if (rights & CH_WRITE)
		retstr.append("W");
	return retstr;
}

std::string FileSystem::typeToText(filetype_t ftype)
{
	if (ftype == ENUM_FILE)
		return std::string("File");
	else
		return std::string("Directory");
}

std::string FileSystem::trimLastSectionOfPath(std::string path)
{
	std::vector<std::string> psplit = split(path);
	if (psplit.size() == 0)
		return std::string("/");
	return path.substr(0, path.size() - psplit.front().size() - 1);
}