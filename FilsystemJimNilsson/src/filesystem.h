#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "memblockdevice.h"

/* Splits a filepath into multiple strings */
std::vector<std::string> split(const std::string &filePath, const char delim = '/');

enum chmod_t : int {CH_READ = 2, CH_WRITE = 4, CH_REMOVE = 1, CH_ALL = 7};
enum filetype_t : int {ENUM_DIRECTORY = 1, ENUM_FILE = 2, ENUM_ERROR = -1};
struct MetaData
{
	MetaData()
	{
		//empty
	}
	MetaData(filetype_t type, std::string name, int location, int size, int rights)
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
	int mSize;
	int mRights;

};
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
    std::string format();
	std::string ls();
    std::string ls(std::string &path);  // optional
    std::string create(std::string &filePath);
	std::string mkdir(std::string& filePath);
	std::string pwd();
    //std::string cat(std::string &fileName) const;
    //std::string save(const std::string &saveFile) const;
    //std::string read(const std::string &saveFile) const;
    std::string rm(std::string &path);
	std::string cd(std::string& dir);
	//std::string copy(const std::string &source, const std::string &dest);

    ///* Optional */
    //std::string append(const std::string &source, const std::string &app);
    //std::string rename(const std::string &source, const std::string &newName);
    //std::string chmod(int permission, const std::string &file);

    /* Add your own member-functions if needed */
private:
	int findLocation(int blockNr, std::string& dirPath);
	MetaData getMetaData(int blockNr, std::string& dirPath);
	int remove(int blockNr, std::string& path);
	int getSize(int blockNr, std::string& path);
	
};

#endif // FILESYSTEM_H
