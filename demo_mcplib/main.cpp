/**
 * @file main.cpp
 * @brief 示例：通过 TOML 配置启动 McpApplication（加载插件、创建线程池对象），再提交任务。
 */
#include "mcpapplication.h"
#include "mcp.h"
#include "imcpthreadpool.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

using namespace std;

static bool fileReadable(const string& path)
{
	ifstream f(path, ios::binary);
	return f.good();
}

#ifdef _WIN32
static string exeDirectory()
{
	char buf[MAX_PATH];
	DWORD n = GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
	if (n == 0 || n >= sizeof(buf)) {
		return {};
	}
	string p(buf, n);
	size_t slash = p.find_last_of("\\/");
	if (slash == string::npos) {
		return {};
	}
	return p.substr(0, slash);
}
#else
static string exeDirectory()
{
	return {};
}
#endif

static bool isAbsolutePath(const string& p)
{
	if (p.empty()) {
		return false;
	}
#ifdef _WIN32
	if (p.size() >= 2 && p[1] == ':') {
		return true;
	}
	if (p.size() >= 2 && p[0] == '\\' && p[1] == '\\') {
		return true;
	}
#endif
	return p[0] == '/';
}

/** 默认同目录 demo.toml；VS 调试时 cwd 常为项目根，故相对路径先 cwd 再 exe 旁。 */
static string resolveConfigPath(const char* userPath)
{
	const char* def = "demo.toml";
	string rel = (userPath && userPath[0]) ? userPath : def;
	if (isAbsolutePath(rel)) {
		return rel;
	}
	if (fileReadable(rel)) {
		return rel;
	}
#ifdef _WIN32
	string dir = exeDirectory();
	if (!dir.empty()) {
		string nextToExe = dir + "\\" + rel;
		if (fileReadable(nextToExe)) {
			return nextToExe;
		}
	}
#endif
	return rel;
}

static atomic<int> g_taskCount(0);

static void simpleTask(int id)
{
	cout << "[Task " << id << "] started on thread " << this_thread::get_id() << endl;
	this_thread::sleep_for(chrono::milliseconds(100));
	g_taskCount++;
	cout << "[Task " << id << "] completed" << endl;
}

int main(int argc, char** argv)
{
	const char* arg = (argc >= 2 && argv[1] && argv[1][0]) ? argv[1] : nullptr;
	string configResolved = resolveConfigPath(arg);
	const char* configPath = configResolved.c_str();

	cout << "========================================" << endl;
	cout << "  MCP demo (TOML + McpApplication)" << endl;
	cout << "  config: " << configPath << endl;
	cout << "========================================" << endl << endl;

	mcp::McpApplication app;
	cout << "[Step 1] Starting from TOML (init MCP, load plugins, create objects)..." << endl;
	if (!app.start(configPath)) {
		vector<string> badPlg, badObj;
		app.getLoadFailedPluginsInfo(badPlg, badObj);
		cerr << "ERROR: McpApplication::start failed" << endl;
		for (const auto& s : badPlg) {
			cerr << "  failed plugin: " << s << endl;
		}
		for (const auto& s : badObj) {
			cerr << "  failed object: " << s << endl;
		}
		return -1;
	}
	cout << "OK" << endl << endl;

	cout << "[Step 2] Lookup demoThreadPool (must match objectName in TOML)..." << endl;
	// 使用 mcp.dll 导出的 FindObject：主程序里的 mcp::api 与 mcpAppliction.dll 内各有一份静态单例，
	// 若用 mcp::api::FindObject 会拿到 exe 侧未初始化的实例。
	auto* pool = static_cast<mcp::IMcpThreadPool*>(mcp::FindObject("demoThreadPool"));
	if (!pool) {
		cerr << "ERROR: FindObject(demoThreadPool) failed" << endl;
		app.stop();
		mcp::Terminate();
		return -1;
	}
	cout << "OK" << endl << endl;

	cout << "[Step 3] start(3)..." << endl;
	pool->start(3);
	cout << "OK: " << pool->getThreadCount() << " threads" << endl << endl;

	cout << "[Step 4] Submit 5 tasks..." << endl;
	for (int i = 0; i < 5; i++) {
		pool->exec([i]() { simpleTask(i); });
		cout << "Submitted task " << i << endl;
	}
	cout << endl;

	cout << "[Step 5] waitForAllDone(3000 ms)..." << endl;
	if (pool->waitForAllDone(3000)) {
		cout << "OK: count=" << g_taskCount.load() << endl;
	} else {
		cout << "WARNING: timeout" << endl;
	}
	cout << endl;

	pool->stop();
	cout << "[Step 6] McpApplication::stop + Terminate..." << endl;
	app.stop();
	mcp::Terminate();
	cout << "OK" << endl;
	return 0;
}
