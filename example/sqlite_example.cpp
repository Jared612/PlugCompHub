#include "pcx.h"
#include "pluginManager.h"
#include "ipcxsqlite.h"

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

	auto* db = static_cast<pcx::IPcxSqlite*>(pcx::api::CreateNamedObject("cpp.pcx.sqlite", "example.sqlite"));
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
	pcx::api::Terminate();
	std::cout << "sqlite example ok\n";
	return 0;
}
