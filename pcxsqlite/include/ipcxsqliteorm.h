/**
 * @file ipcxsqliteorm.h
 * @brief 基于 SQLite 的键值表 ORM 抽象（单表 KV）。
 * @details 典型实现使用固定 schema 存 key/value；适合轻量配置或缓存，非通用关系 ORM。
 */
#pragma once

#include <string>

namespace pcx {

/**
 * @brief 键值存储接口（表级封装）。
 * @note `open` 成功后再调用其它接口；并发由调用方协调。
 */
class IPcxSqliteOrm
{
public:
	virtual ~IPcxSqliteOrm() = default;

	/**
	 * @brief 打开数据库并准备 KV 表结构。
	 * @param dbPath 数据库文件路径。
	 * @param[out] error 失败原因。
	 * @return 成功 true。
	 */
	virtual bool open(const std::string& dbPath, std::string& error) = 0;

	/** @brief 关闭数据库连接。 */
	virtual void close() = 0;

	/** @return 是否处于打开状态。 */
	virtual bool isOpen() const = 0;

	/**
	 * @brief 写入或更新一条记录。
	 * @param key 键。
	 * @param value 值。
	 * @param[out] error 失败原因。
	 * @return 成功 true。
	 */
	virtual bool put(const std::string& key, const std::string& value, std::string& error) = 0;

	/**
	 * @brief 按键读取值。
	 * @param key 键。
	 * @param[out] value 输出值；未找到时由实现决定 `value` 是否清空。
	 * @param[out] error 失败或「未找到」说明。
	 * @return 成功 true。
	 */
	virtual bool get(const std::string& key, std::string& value, std::string& error) = 0;

	/**
	 * @brief 删除指定键。
	 * @param[out] error 失败原因。
	 * @return 成功 true（删除 0 行也可能视为成功，以实现为准）。
	 */
	virtual bool erase(const std::string& key, std::string& error) = 0;

	/**
	 * @brief 返回当前 KV 条数。
	 * @param[out] error 失败原因。
	 * @return 行数；失败返回负值（若实现无法区分，可查 `error`）。
	 */
	virtual int count(std::string& error) = 0;
};

}
