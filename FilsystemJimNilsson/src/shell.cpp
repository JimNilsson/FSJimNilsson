#include <iostream>
#include <sstream>
#include "filesystem.h"

using namespace std;

const int MAXCOMMANDS = 8;
const int NUMAVAILABLECOMMANDS = 17;

string availableCommands[NUMAVAILABLECOMMANDS] = {
    "quit","format","ls","create","cat","save","read",
    "rm","copy","append","rename","mkdir","cd","pwd","help","dumpblocks","chmod",
};

/* Takes usercommand from input and returns number of commands, commands are stored in strArr[] */
int parseCommandString(const string &userCommand, string strArr[]);
int findCommand(string &command);



string help();

int main(void) {

    string userCommand, commandArr[MAXCOMMANDS];
    string user = "user@DV1492";    // Change this if you want another user to be displayed
    string currentDir = "/";    // current directory, used for output
	FileSystem fileSystem;

    bool bRun = true;

    do {
        cout << user << ":" << currentDir << "$ ";
        getline(cin, userCommand);

        int nrOfCommands = parseCommandString(userCommand, commandArr);
        if (nrOfCommands > 0) {

            int cIndex = findCommand(commandArr[0]);
            switch(cIndex) {

            case 0: // quit
                bRun = false;
                cout << "Exiting\n";
                break;
            case 1: // format
				cout << fileSystem.format();
                break;
            case 2: // ls
                cout << "Listing directory" << endl;
				if (nrOfCommands == 1)
					cout << fileSystem.ls();
				else
					cout << fileSystem.ls(commandArr[1]);
                break;
            case 3: // create
			{
				std::string trywrite = fileSystem.create(commandArr[1], ENUM_FILE);
				cout << trywrite;
				//Request initial data to be written to file
				//But only if FileSystem::create succeeded.
				if (trywrite.substr(0, 3).compare("New") == 0)
				{
					std::cout << "Enter data to be written to new file:\n";
					std::string input("");
					do
					{
						if (input.size() > 0)
							std::cout << "File too large to be written, try again\n";
						std::getline(std::cin, input);

					} while (fileSystem.appendString(commandArr[1], input) < 0);
				}
			}
                break;
            case 4: // cat
				cout << fileSystem.cat(commandArr[1]) << endl;
                break;
            case 5: // save
				cout << fileSystem.save(commandArr[1]);
                break;
            case 6: // read
				cout << fileSystem.read(commandArr[1]);
                break;
            case 7: // rm
				cout << fileSystem.rm(commandArr[1]);
                break;

            case 8: // copy
				cout << fileSystem.copy(commandArr[1], commandArr[2]);
                break;

            case 9: // append
				cout << fileSystem.append(commandArr[1], commandArr[2]);
                break;

            case 10: // rename
				cout << fileSystem.rename(commandArr[1], commandArr[2]);
                break;

            case 11: // mkdir
				cout << fileSystem.create(commandArr[1], ENUM_DIRECTORY);
                break;

            case 12: // cd
				cout << fileSystem.cd(commandArr[1]);
                break;

            case 13: // pwd
				cout << fileSystem.pwd();
                break;

            case 14: // help
                cout << help() << endl;
                break;
			case 15: //Dump "harddrive" to file, used for debugging
				fileSystem.dumpHarddrive();
				break;
			case 16: //chmod
				try {
					cout << fileSystem.chmod((chmod_t)stoi(commandArr[1]), commandArr[2]);
				}
				catch (std::invalid_argument)
				{
					cout << "Invalid argument supplied.\n";
				}
				break;
            default:
                cout << "Unknown command: " << commandArr[0] << endl;

            }
        }
    } while (bRun == true);

    return 0;
}

int parseCommandString(const string &userCommand, string strArr[]) {
    stringstream ssin(userCommand);
    int counter = 0;
    while (ssin.good() && counter < MAXCOMMANDS) {
        ssin >> strArr[counter];
        counter++;
    }
    if (strArr[0] == "") {
        counter = 0;
    }
    return counter;
}
int findCommand(string &command) {
    int index = -1;
    for (int i = 0; i < NUMAVAILABLECOMMANDS && index == -1; ++i) {
        if (command == availableCommands[i]) {
            index = i;
        }
    }
    return index;
}

std::vector<string> split(const string &filePath, const char delim) {
    vector<string> output;
    string cpy = filePath;

    size_t end = cpy.find_last_of(delim);
    if (cpy.length() > end+1) {
        output.push_back(cpy.substr(end+1, cpy.length()));
    }

    while (end != 0 && end!= std::string::npos) {

        cpy = cpy.substr(0, cpy.find_last_of('/'));
        //cout << cpy << endl;
        end = cpy.find_last_of(delim);
        output.push_back(cpy.substr(end+1, cpy.length()));

    }

    return output;
}

string help() {
    string helpStr;
    helpStr += "OSD Disk Tool .oO Help Screen Oo.\n";
    helpStr += "-----------------------------------------------------------------------------------\n" ;
    helpStr += "* quit:                             Quit OSD Disk Tool\n";
    helpStr += "* format;                           Formats disk\n";
    helpStr += "* ls     <path>:                    Lists contents of <path>.\n";
    helpStr += "* create <path>:                    Creates a file and stores contents in <path>\n";
    helpStr += "* cat    <path>:                    Dumps contents of <file>.\n";
    helpStr += "* save   <real-file>:               Saves disk to <real-file>\n";
    helpStr += "* read   <real-file>:               Reads <real-file> onto disk\n";
    helpStr += "* rm     <file>:                    Removes <file>\n";
    helpStr += "* copy   <source>    <destination>: Copy <source> to <destination>\n";
    helpStr += "* append <source>    <destination>: Appends contents of <source> to <destination>\n";
    helpStr += "* rename <old-file>  <new-file>:    Renames <old-file> to <new-file>\n";
    helpStr += "* mkdir  <directory>:               Creates a new directory called <directory>\n";
    helpStr += "* cd     <directory>:               Changes current working directory to <directory>\n";
	helpStr += "* chmod  <permission> <file>:       Changes permission. 0=NONE,2=R,4=W,6=RW\n";
	helpStr += "* dumpblocks:                       Dumps HD to datadump.txt and rawdatadump.txt\n";
    helpStr += "* pwd:                              Get current working directory\n";
    helpStr += "* help:                             Prints this help screen\n";
    return helpStr;
}
