#pragma once

#include <string>

namespace pcx {

class IPcxSqliteOrm
{
public:
	virtual ~IPcxSqliteOrm() = default;

	virtual bool open(const std::string& dbPath, std::string& error) = 0;
	virtual void close() = 0;
	virtual bool isOpen() const = 0;

	virtual bool put(const std::string& key, const std::string& value, std::string& error) = 0;
	virtual bool get(const std::string& key, std::string& value, std::string& error) = 0;
	virtual bool erase(const std::string& key, std::string& error) = 0;
	virtual int count(std::string& error) = 0;
};

}
