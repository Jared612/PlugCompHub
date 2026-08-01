#include "core.h"
#include "interface.h"
#include "isqliteorm.h"
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

	auto* orm = static_cast<pch::ISqliteOrm*>(pch::api::CreateNamedObject("cpp.pch.sqliteorm", "example.sqliteorm"));
	if (orm == nullptr) {
		std::cerr << "create sqlite_orm object failed\n";
		return 1;
	}

	std::string err;
	if (!orm->open(":memory:", err)) {
		std::cerr << "open sqlite_orm failed: " << err << "\n";
		return 1;
	}

	if (!orm->put("k1", "v1", err) || !orm->put("k2", "v2", err) || !orm->put("k1", "v1-update", err)) {
		std::cerr << "put failed: " << err << "\n";
		return 1;
	}

	std::string val;
	if (!orm->get("k1", val, err) || val != "v1-update") {
		std::cerr << "get failed: " << err << ", value=" << val << "\n";
		return 1;
	}

	const int c1 = orm->count(err);
	if (c1 != 2) {
		std::cerr << "count failed: " << err << ", count=" << c1 << "\n";
		return 1;
	}

	if (!orm->erase("k2", err)) {
		std::cerr << "erase failed: " << err << "\n";
		return 1;
	}

	const int c2 = orm->count(err);
	if (c2 != 1) {
		std::cerr << "count after erase failed: " << err << ", count=" << c2 << "\n";
		return 1;
	}

	orm->close();
	pch::api::Terminate();
	std::cout << "sqlite_orm example ok\n";
	return 0;
}
