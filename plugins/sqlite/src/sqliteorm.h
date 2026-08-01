#pragma once

#include "isqliteorm.h"

#include <memory>
#include <mutex>
#include <string>

namespace pch {

class SqliteOrm : public ISqliteOrm
{
public:
	SqliteOrm();
	~SqliteOrm() override;

	bool open(const std::string& dbPath, std::string& error) override;
	void close() override;
	bool isOpen() const override;

	bool put(const std::string& key, const std::string& value, std::string& error) override;
	bool get(const std::string& key, std::string& value, std::string& error) override;
	bool erase(const std::string& key, std::string& error) override;
	int count(std::string& error) override;

private:
	struct Impl;
	mutable std::mutex _mutex;
	std::unique_ptr<Impl> _impl;
};

}
