#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "memblockdevice.h"
#include "MetaData.h"
#include <algorithm>

/* Splits a filepath into multiple strings */
std::vector<std::string> split(const std::string &filePath, const char delim = '/');


class FileSystem
{
private:
    MemBlockDevice mMemblockDevice;
	char pRAM[512]; //helpful for loading blocks into "memory"
	
	
    // Here you can add your own data structures
public:
    FileSystem();
	std::string mCurrentDir;
    /* These commands needs to implemented */
    /*
     * However, feel free to change the signatures, these are just examples.
     * Remember to remove 'const' if needed.
     */
	void dumpHarddrive();

    std::string format();
	std::string ls(); //Prints contents of current directory
    std::string ls(std::string path);  // optional
    std::string create(std::string path, filetype_t type); //Creates file or directory
	std::string pwd();
    std::string cat(std::string path);
	/* Saves an image of the harddrive */
    std::string save(const std::string &saveFile);
	/* Loads an image of the harddrive */
    std::string read(const std::string &saveFile);
    std::string rm(std::string path, bool ignorePermission = false);
	std::string rmrf(std::string path);
	std::string cd(std::string dir);
	std::string copy(std::string source, std::string dest);
	int appendString(std::string path, std::string content);

    ///* Optional */
    std::string append(std::string sourcefile, std::string destfile);
    std::string rename(std::string source, std::string newName);
    std::string chmod(chmod_t permission, std::string filepath);
	std::string getCurrentDir() { return mCurrentDir; }//cd is equivalent of setcurrentdir
    /* Add your own member-functions if needed */
private:
	int findLocation(int blockNr, std::string& dirPath, int seekLength = sizeof(MetaData));
	int findParentLocation(int blockNr, std::string& path, int seekLength = sizeof(MetaData));
	MetaData getMetaData(int blockNr, std::string& dirPath, int seekLength = sizeof(MetaData));
	int findMetaDataLocation(int blockNr, std::string path, int seekLength = sizeof(MetaData));
	int getSize(std::string path);
	std::string pathToAbsolutePath(std::string path);
	std::string rightsToText(chmod_t rights);
	std::string typeToText(filetype_t ftype);
	std::string trimLastSectionOfPath(std::string path);
	int changeMetaData(std::string filepath,const MetaData& md);
	/* returns the position of the metadata in a block*/
	int findMetaDataInBlock(const MetaData& md, int blockNr);
	
};

#endif // FILESYSTEM_H
