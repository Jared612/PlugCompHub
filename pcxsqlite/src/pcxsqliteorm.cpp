#include "pcxsqliteorm.h"

#include "kvtabledefine.h"
#include "sqlite_orm.h"

#include <utility>

namespace pcx {

namespace {

struct KvRecord
{
	std::string key;
	std::string value;
	std::string create_time;
	std::string modify_time;
};

/** 与 PluginApp `GET_KV_MODEL_STORAGE` 同构：按路径生成 storage（表名、列名由宏统一） */
#define PCX_GET_KV_STORAGE(dbPath)                                                                                    \
	sqlite_orm::make_storage(                                                                                        \
		dbPath,                                                                                                      \
		sqlite_orm::make_table(                                                                                      \
			PCX_KV_TABLE_NAME,                                                                                       \
			sqlite_orm::make_column(PCX_KV_COL_KEY, &KvRecord::key, sqlite_orm::primary_key()),                      \
			sqlite_orm::make_column(PCX_KV_COL_VALUE, &KvRecord::value),                                             \
			sqlite_orm::make_column(PCX_KV_COL_CREATE_TIME, &KvRecord::create_time),                               \
			sqlite_orm::make_column(PCX_KV_COL_MODIFY_TIME, &KvRecord::modify_time)))

inline auto makeKvStorage(const std::string& dbPath)
{
	return PCX_GET_KV_STORAGE(dbPath);
}

using Storage = decltype(makeKvStorage(std::declval<std::string>()));
}

struct PcxSqliteOrm::Impl
{
	explicit Impl(const std::string& dbPath)
		: storage(makeKvStorage(dbPath))
	{
	}

	Storage storage;
};

PcxSqliteOrm::PcxSqliteOrm() = default;

PcxSqliteOrm::~PcxSqliteOrm() = default;

bool PcxSqliteOrm::open(const std::string& dbPath, std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	try {
		_impl.reset(new Impl(dbPath));
		_impl->storage.sync_schema();
		return true;
	}
	catch (const std::exception& ex) {
		_impl.reset();
		error = ex.what();
		return false;
	}
}

void PcxSqliteOrm::close()
{
	std::lock_guard<std::mutex> lock(_mutex);
	_impl.reset();
}

bool PcxSqliteOrm::isOpen() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _impl != nullptr;
}

bool PcxSqliteOrm::put(const std::string& key, const std::string& value, std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	if (_impl == nullptr) {
		error = "sqlite_orm storage is not open";
		return false;
	}

	try {
		auto& st = _impl->storage;
		auto localTime = st.select(sqlite_orm::datetime("now", "localtime")).front();

		auto existing = st.get_pointer<KvRecord>(key);
		const std::string createTime = (existing != nullptr) ? existing->create_time : localTime;
		KvRecord model{ key, value, createTime, localTime };
		st.replace(model);
		return true;
	}
	catch (const std::exception& ex) {
		error = ex.what();
		return false;
	}
}

bool PcxSqliteOrm::get(const std::string& key, std::string& value, std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	value.clear();
	if (_impl == nullptr) {
		error = "sqlite_orm storage is not open";
		return false;
	}

	try {
		auto rec = _impl->storage.get<KvRecord>(sqlite_orm::where(sqlite_orm::is_equal(&KvRecord::key, key)));
		value = rec.value;
		return true;
	}
	catch (const std::exception& ex) {
		error = ex.what();
		return false;
	}
}

bool PcxSqliteOrm::erase(const std::string& key, std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	if (_impl == nullptr) {
		error = "sqlite_orm storage is not open";
		return false;
	}

	try {
		_impl->storage.remove_all<KvRecord>(sqlite_orm::where(sqlite_orm::is_equal(&KvRecord::key, key)));
		return true;
	}
	catch (const std::exception& ex) {
		error = ex.what();
		return false;
	}
}

int PcxSqliteOrm::count(std::string& error)
{
	std::lock_guard<std::mutex> lock(_mutex);
	error.clear();
	if (_impl == nullptr) {
		error = "sqlite_orm storage is not open";
		return -1;
	}

	try {
		return static_cast<int>(_impl->storage.count<KvRecord>());
	}
	catch (const std::exception& ex) {
		error = ex.what();
		return -1;
	}
}

}
