#include "core.h"
#include "pluginManager.h"
#include "isqliteorm.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
static std::string exeDir()
{
	char buf[MAX_PATH];
	DWORD n = GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
	if (n == 0 || n >= sizeof(buf)) return ".";
	std::string p(buf, n);
	auto pos = p.find_last_of("\\/");
	return (pos == std::string::npos) ? "." : p.substr(0, pos);
}
static std::string joinPath(const std::string& d, const char* f) { return d + "\\" + f; }
#else
static std::string exeDir() { return "."; }
static std::string joinPath(const std::string& d, const char* f) { return d + "/" + f; }
#endif

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, "PCH.dll").c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::PluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, "pchsqlite.dll").c_str()) == nullptr) {
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
