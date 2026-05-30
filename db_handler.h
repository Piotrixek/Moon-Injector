#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <vector>

struct Workspace
{
    int id;
    std::string name;
};

struct DllInfo
{
    std::string path;
};

class DBHandler
{
  public:
    DBHandler(const std::string &dbPath);

    std::vector<Workspace> getWorkspaces();
    long long addWorkspace(const std::string &name);
    void renameWorkspace(int id, const std::string &newName);
    void deleteWorkspace(int id);

    std::vector<DllInfo> getDlls(int workspaceId);
    void syncDlls(int workspaceId, const std::vector<std::string> &paths);

  private:
    SQLite::Database db;
};
