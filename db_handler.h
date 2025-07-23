#pragma once
#include <string>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>

// this struct will hold our dll info
struct DllInfo {
    std::string path;
};

class DBHandler {
public:
    // sets up the db and the table if they dont exist
    DBHandler(const std::string& dbPath);

    // gets all the dlls we've saved
    std::vector<DllInfo> getDlls();

    // adds a new dll to our list
    void addDll(const std::string& path);

    // removes a specific dll by its path
    void removeDll(const std::string& path);

    // wipes all the dlls from the list
    void clearDlls();

private:
    // our connection to the db file
    SQLite::Database db;
};
