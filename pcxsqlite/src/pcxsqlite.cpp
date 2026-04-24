#include "pcxsqlite.h"

#include "sqlite3.h"

namespace pcx {

PcxSqlite::~PcxSqlite()
{
	close();
}

bool PcxSqlite::open(const std::string& dbPath)
{
	// 连接句柄与后续 exec/query 共用互斥，避免多线程并发 sqlite3_* 未定义行为
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

void PcxSqlite::close()
{
	std::lock_guard<std::mutex> lock(_mutex);
	closeUnlocked();
}

bool PcxSqlite::isOpen() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _db != nullptr;
}

int PcxSqlite::exec(const std::string& sql, std::string& error)
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

bool PcxSqlite::queryInt(const std::string& sql, int& value, std::string& error)
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

bool PcxSqlite::queryString(const std::string& sql, std::string& value, std::string& error)
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

void PcxSqlite::closeUnlocked()
{
	if (_db != nullptr) {
		sqlite3_close(_db);
		_db = nullptr;
	}
}

}
