#include "core/core.h"
#include "core/interface.h"
#include "sqlite/isqlite.h"
#include "example_common.h"

#include <iostream>
#include <string>

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::IPluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, PCH_SQLITE_LIB).c_str()) == nullptr) {
		std::cerr << "load pchsqlite plugin failed\n";
		return 1;
	}

	auto* db = static_cast<pch::ISqlite*>(pch::api::CreateNamedObject("cpp.pch.sqlite", "example.sqlite"));
	if (db == nullptr) {
		std::cerr << "create sqlite object failed\n";
		return 1;
	}

	if (!db->open(":memory:")) {
		std::cerr << "open sqlite failed\n";
		return 1;
	}

	std::string err;
	if (db->exec("CREATE TABLE users(id INTEGER PRIMARY KEY, name TEXT);", err) != 0) {
		std::cerr << "create table failed: " << err << "\n";
		return 1;
	}
	if (db->exec("INSERT INTO users(name) VALUES('alice'),('bob');", err) != 0) {
		std::cerr << "insert failed: " << err << "\n";
		return 1;
	}

	int count = 0;
	if (!db->queryInt("SELECT COUNT(*) FROM users;", count, err) || count != 2) {
		std::cerr << "query count failed: " << err << ", count=" << count << "\n";
		return 1;
	}

	std::string firstName;
	if (!db->queryString("SELECT name FROM users ORDER BY id LIMIT 1;", firstName, err) || firstName != "alice") {
		std::cerr << "query name failed: " << err << ", name=" << firstName << "\n";
		return 1;
	}

	db->close();
	pch::api::Terminate();
	std::cout << "sqlite example ok\n";
	return 0;
}
