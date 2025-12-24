#pragma once
#include "sqlite3.h" // Ensure sqlite3 is linked in the project
#include <string>
#include <vector>

struct PhoneNumberRecord {
  int id;
  std::string number;
  std::string title;
  std::string url;
  std::string timestamp;
};

class Database {
public:
  Database(const std::string &dbPath);
  ~Database();

  bool init();
  bool saveNumber(const std::string &number, const std::string &title,
                  const std::string &url);

  // Helper for main.cpp compatibility
  bool SaveNumber(const std::string &number) {
    return saveNumber(number, "", "");
  }

  std::vector<PhoneNumberRecord> getAllRecords();
  bool exportToCSV(const std::string &filepath);

private:
  sqlite3 *db = nullptr;
  std::string dbPath;

  bool executeQuery(const std::string &query);
};
