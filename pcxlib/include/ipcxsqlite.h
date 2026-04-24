#pragma once

#include <string>

namespace pcx {

class IPcxSqlite
{
public:
	virtual ~IPcxSqlite() = default;

	virtual bool open(const std::string& dbPath) = 0;
	virtual void close() = 0;
	virtual bool isOpen() const = 0;

	virtual int exec(const std::string& sql, std::string& error) = 0;
	virtual bool queryInt(const std::string& sql, int& value, std::string& error) = 0;
	virtual bool queryString(const std::string& sql, std::string& value, std::string& error) = 0;
};

}
