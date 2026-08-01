#include "sqlite.h"

#include "sqlite3.h"

namespace pch {

Sqlite::~Sqlite()
{
	close();
}

bool Sqlite::open(const std::string& dbPath)
{
	// 确保函数是线程安全的 SQL 操作
	// 所有 exec/query 操作共享 _mutex 保护 sqlite3_* API
	std::lock_guard<std::mutex> lock(_mutex);
	closeUnlocked();

	sqlite3* db = nullptr;
	const int rc = sqlite3_open(dbPath.c_str(), &db);
	if (rc != SQLITE_OK) {
		if (db != nullptr) {
			sqlite3_close(db);
		}
		_db = nullptr;
		return false;
	}

	_db = db;
	return true;
}

void Sqlite::close()
{
	std::lock_guard<std::mutex> lock(_mutex);
	closeUnlocked();
}

bool Sqlite::isOpen() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _db != nullptr;
}

int Sqlite::exec(const std::string& sql, std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	if (_db == nullptr) {
		error = "sqlite database is not open";
		return SQLITE_MISUSE;
	}

	char* errMsg = nullptr;
	const int rc = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK && errMsg != nullptr) {
		error.assign(errMsg);
		sqlite3_free(errMsg);
	}
	return rc;
}

bool Sqlite::queryInt(const std::string& sql, int& value, std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	if (_db == nullptr) {
		error = "sqlite database is not open";
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(_db);
		return false;
	}

	const int stepRc = sqlite3_step(stmt);
	if (stepRc != SQLITE_ROW) {
		error = (stepRc == SQLITE_DONE) ? "no rows returned" : sqlite3_errmsg(_db);
		sqlite3_finalize(stmt);
		return false;
	}

	value = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return true;
}

bool Sqlite::queryString(const std::string& sql, std::string& value, std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	value.clear();
	if (_db == nullptr) {
		error = "sqlite database is not open";
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(_db);
		return false;
	}

	const int stepRc = sqlite3_step(stmt);
	if (stepRc != SQLITE_ROW) {
		error = (stepRc == SQLITE_DONE) ? "no rows returned" : sqlite3_errmsg(_db);
		sqlite3_finalize(stmt);
		return false;
	}

	const unsigned char* text = sqlite3_column_text(stmt, 0);
	value = (text == nullptr) ? "" : reinterpret_cast<const char*>(text);
	sqlite3_finalize(stmt);
	return true;
}

void Sqlite::closeUnlocked()
{
	if (_db != nullptr) {
		sqlite3_close(_db);
		_db = nullptr;
	}
}

}
