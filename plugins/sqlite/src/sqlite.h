#pragma once

#include "isqlite.h"

#include <mutex>
#include <string>

struct sqlite3;

namespace pch {

class Sqlite : public ISqlite
{
public:
	Sqlite() = default;
	~Sqlite() override;

	bool open(const std::string& dbPath) override;
	void close() override;
	bool isOpen() const override;

	int exec(const std::string& sql, std::string& error) override;
	bool queryInt(const std::string& sql, int& value, std::string& error) override;
	bool queryString(const std::string& sql, std::string& value, std::string& error) override;

private:
	void closeUnlocked();

private:
	mutable std::mutex _mutex;
	sqlite3* _db = nullptr;
};

}
