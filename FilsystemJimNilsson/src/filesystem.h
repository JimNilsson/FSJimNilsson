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
	std::string mCurrentDir;
	
    // Here you can add your own data structures
public:
    FileSystem();

    /* These commands needs to implemented */
    /*
     * However, feel free to change the signatures, these are just examples.
     * Remember to remove 'const' if needed.
     */
	void dumpHarddrive();

    std::string format();
	std::string ls(); //Prints contents of current directory
    std::string ls(std::string &path);  // optional
    std::string create(std::string path, filetype_t type); //Creates file or directory
	std::string pwd();
    //std::string cat(std::string &fileName) const;
    //std::string save(const std::string &saveFile) const;
    //std::string read(const std::string &saveFile) const;
    std::string rm(std::string &path);
	std::string cd(std::string& dir);
	//std::string copy(const std::string &source, const std::string &dest);
	int appendString(std::string path, std::string content);

    ///* Optional */
    std::string append(std::string sourcefile, std::string destfile);
    //std::string rename(const std::string &source, const std::string &newName);
    //std::string chmod(int permission, const std::string &file);

    /* Add your own member-functions if needed */
private:
	int findLocation(int blockNr, std::string& dirPath, int seekLength = sizeof(MetaData));
	int findParentLocation(int blockNr, std::string& path, int seekLength = sizeof(MetaData));
	MetaData getMetaData(int blockNr, std::string& dirPath, int seekLength = sizeof(MetaData));
	int remove(int blockNr, std::string& path);
	int getSize(int blockNr, std::string& path);
	
};

#endif // FILESYSTEM_H
