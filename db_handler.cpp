#include "db_handler.h"
#include <iostream>

DBHandler::DBHandler(const std::string& dbPath) : db(dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
    try {
        db.exec("PRAGMA foreign_keys = ON;");

        db.exec("CREATE TABLE IF NOT EXISTS workspaces ("
            "id INTEGER PRIMARY KEY, "
            "name TEXT NOT NULL UNIQUE)");

        db.exec("CREATE TABLE IF NOT EXISTS dlls ("
            "id INTEGER PRIMARY KEY, "
            "path TEXT NOT NULL, "
            "workspace_id INTEGER NOT NULL, "
            "FOREIGN KEY(workspace_id) REFERENCES workspaces(id) ON DELETE CASCADE, "
            "UNIQUE(path, workspace_id))");

        if (getWorkspaces().empty()) {
            addWorkspace("Default");
        }
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception during initialization: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Workspace> DBHandler::getWorkspaces() {
    std::vector<Workspace> workspaces;
    try {
        SQLite::Statement query(db, "SELECT id, name FROM workspaces ORDER BY name");
        while (query.executeStep()) {
            workspaces.push_back({ query.getColumn(0).getInt(), query.getColumn(1).getString() });
        }
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception in getWorkspaces: " << e.what() << std::endl;
    }
    return workspaces;
}

long long DBHandler::addWorkspace(const std::string& name) {
    try {
        SQLite::Statement query(db, "INSERT OR IGNORE INTO workspaces (name) VALUES (?)");
        query.bind(1, name);
        int changes = query.exec();
        if (changes > 0) {
            return db.getLastInsertRowid();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception in addWorkspace: " << e.what() << std::endl;
    }
    return 0;
}

void DBHandler::renameWorkspace(int id, const std::string& newName) {
    try {
        SQLite::Statement query(db, "UPDATE workspaces SET name = ? WHERE id = ?");
        query.bind(1, newName);
        query.bind(2, id);
        query.exec();
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception in renameWorkspace: " << e.what() << std::endl;
    }
}

void DBHandler::deleteWorkspace(int id) {
    try {
        SQLite::Statement query(db, "DELETE FROM workspaces WHERE id = ?");
        query.bind(1, id);
        query.exec();
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception in deleteWorkspace: " << e.what() << std::endl;
    }
}

std::vector<DllInfo> DBHandler::getDlls(int workspaceId) {
    std::vector<DllInfo> dlls;
    try {
        SQLite::Statement query(db, "SELECT path FROM dlls WHERE workspace_id = ?");
        query.bind(1, workspaceId);
        while (query.executeStep()) {
            dlls.push_back({ query.getColumn(0).getString() });
        }
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception in getDlls: " << e.what() << std::endl;
    }
    return dlls;
}

void DBHandler::syncDlls(int workspaceId, const std::vector<std::string>& paths) {
    try {
        db.exec("BEGIN");

        SQLite::Statement del_query(db, "DELETE FROM dlls WHERE workspace_id = ?");
        del_query.bind(1, workspaceId);
        del_query.exec();

        SQLite::Statement ins_query(db, "INSERT OR IGNORE INTO dlls (path, workspace_id) VALUES (?, ?)");
        for (const auto& path : paths) {
            ins_query.bind(1, path);
            ins_query.bind(2, workspaceId);
            ins_query.exec();
            ins_query.reset();
        }

        db.exec("COMMIT");
    }
    catch (const std::exception& e) {
        std::cerr << "SQLite exception in syncDlls: " << e.what() << std::endl;
        try {
            db.exec("ROLLBACK");
        }
        catch (const std::exception& e2) {
            std::cerr << "SQLite exception during ROLLBACK: " << e2.what() << std::endl;
        }
    }
}