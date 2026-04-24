#include "pcx.h"
#include "pluginManager.h"
#include "ipcxsqliteorm.h"

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
	if (pcx::api::Initialize(joinPath(dir, "pcx.dll").c_str()) != PCX_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pcx::PluginManager*>(pcx::api::FindObject(PCX_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, "pcxsqlite.dll").c_str()) == nullptr) {
		std::cerr << "load pcxsqlite plugin failed\n";
		return 1;
	}

	auto* orm = static_cast<pcx::IPcxSqliteOrm*>(pcx::api::CreateNamedObject("cpp.pcx.sqliteorm", "example.sqliteorm"));
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
	pcx::api::Terminate();
	std::cout << "sqlite_orm example ok\n";
	return 0;
}
