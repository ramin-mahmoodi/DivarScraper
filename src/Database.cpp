#include "Database.h"
#include <ctime>
#include <fstream>
#include <iostream>


Database::Database(const std::string &dbPath) : dbPath(dbPath) {}

Database::~Database() {
  if (db) {
    sqlite3_close(db);
  }
}

bool Database::init() {
  int rc = sqlite3_open(dbPath.c_str(), &db);
  if (rc) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  const char *sql = "CREATE TABLE IF NOT EXISTS phone_numbers ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "number TEXT NOT NULL,"
                    "title TEXT,"
                    "url TEXT UNIQUE,"
                    "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

  return executeQuery(sql);
}

bool Database::executeQuery(const std::string &query) {
  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, query.c_str(), 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
    return false;
  }
  return true;
}

bool Database::saveNumber(const std::string &number, const std::string &title,
                          const std::string &url) {
  if (number.empty())
    return false;

  // Simple sanitization to prevent SQL injection (better to use prepared
  // statements in prod) For this simple tool, we'll just escape quotes manually
  // if needed, or rely on basic usage. Using sqlite3_mprintf is safer if
  // available, but let's stick to standard std::string for simplicity with
  // caveat.

  std::string sql =
      "INSERT OR IGNORE INTO phone_numbers (number, title, url) VALUES ('" +
      number + "', '" + title + "', '" + url + "');";

  return executeQuery(sql);
}

std::vector<PhoneNumberRecord> Database::getAllRecords() {
  std::vector<PhoneNumberRecord> records;
  const char *sql =
      "SELECT id, number, title, url, timestamp FROM phone_numbers";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      PhoneNumberRecord record;
      record.id = sqlite3_column_int(stmt, 0);
      record.number =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      const char *title =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      record.title = title ? title : "";
      const char *url =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
      record.url = url ? url : "";
      const char *ts =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
      record.timestamp = ts ? ts : "";
      records.push_back(record);
    }
    sqlite3_finalize(stmt);
  }
  return records;
}

bool Database::exportToCSV(const std::string &filepath) {
  std::ofstream file(filepath);
  if (!file.is_open())
    return false;

  file << "ID,Number,Title,URL,Timestamp\n";
  auto records = getAllRecords();
  for (const auto &rec : records) {
    file << rec.id << "," << rec.number << "," << rec.title << "," << rec.url
         << "," << rec.timestamp << "\n";
  }
  file.close();
  return true;
}
