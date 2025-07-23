#include "db_handler.h"
#include <iostream>

// constructor, opens the db and makes the table
DBHandler::DBHandler(const std::string& dbPath) : db(dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
    try {
        // sql command to create the 'dlls' table if it's not already there
        // it has an id and a path for the dll
        db.exec("CREATE TABLE IF NOT EXISTS dlls (id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT NOT NULL UNIQUE)");
    }
    catch (const std::exception& e) {
        // if something goes wrong we'll see it in the console
        std::cerr << "SQLite exception: " << e.what() << std::endl;
    }
}

// gets all the dlls from the table
std::vector<DllInfo> DBHandler::getDlls() {
    std::vector<DllInfo> dlls;
    try {
        // this query just selects all the paths from our table
        SQLite::Statement query(db, "SELECT path FROM dlls");
        while (query.executeStep()) {
            // add each dll we find to our list
            dlls.push_back({ query.getColumn(0).getString() });
        }
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception: " << e.what() << std::endl;
    }
    return dlls;
}

// adds a new dll path to the db
void DBHandler::addDll(const std::string& path) {
    try {
        // start a transaction to make sure the data saves
        SQLite::Transaction transaction(db);
        // prepares the sql to insert a new path
        // the 'OR IGNORE' part means it wont cry if we try to add a duplicate
        SQLite::Statement query(db, "INSERT OR IGNORE INTO dlls (path) VALUES (?)");
        query.bind(1, path);
        query.exec();
        // commit the changes to the file
        transaction.commit();
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception: " << e.what() << std::endl;
    }
}

// removes a dll by its path
void DBHandler::removeDll(const std::string& path) {
    try {
        // wrap this in a transaction too
        SQLite::Transaction transaction(db);
        SQLite::Statement query(db, "DELETE FROM dlls WHERE path = ?");
        query.bind(1, path);
        query.exec();
        // and commit
        transaction.commit();
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception: " << e.what() << std::endl;
    }
}


// clears the whole table
void DBHandler::clearDlls() {
    try {
        // same here start a transaction
        SQLite::Transaction transaction(db);
        // just deletes everything from the dlls table
        db.exec("DELETE FROM dlls");
        // and commit it
        transaction.commit();
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception: " << e.what() << std::endl;
    }
}