/**
 * @file iSqlite.h
 * @brief PCH SQLite 组件对外抽象：打开数据库、执�?SQL、简单标量查询�? * @details 实现通常基于 sqlite3 C API；错误信息通过 `error` 字符串返回给调用方�? */
#pragma once

#include <string>

namespace pch {

/**
 * @brief SQLite 访问封装（非 ORM）�? * @note 具体线程安全策略以实现为准；多线程共享同一实例时应在业务层加锁或每线程独立实例�? */
class ISqlite
{
public:
	virtual ~ISqlite() = default;

	/**
	 * @brief 打开或创建数据库文件�?	 * @param dbPath 本地数据库路径�?	 * @return 成功 true；失�?false（具体原因见实现侧日志或后续 `exec` 返回码）�?	 */
	virtual bool open(const std::string& dbPath) = 0;

	/** @brief 关闭连接并释放底层句柄�?*/
	virtual void close() = 0;

	/** @return 是否已打开有效连接�?*/
	virtual bool isOpen() const = 0;

	/**
	 * @brief 执行任意 SQL（DDL/DML，无结果集或忽略结果集）�?	 * @param sql SQL 文本�?	 * @param[out] error 失败时人类可读说明�?	 * @return SQLite 风格返回码；0 表示成功（与 sqlite3_exec 语义对齐，具体以实现为准）�?	 */
	virtual int exec(const std::string& sql, std::string& error) = 0;

	/**
	 * @brief 查询单个整数（如 `SELECT count(*)`）�?	 * @param sql 单标量查询语句�?	 * @param[out] value 输出列值�?	 * @param[out] error 失败说明�?	 * @return 成功 true�?	 */
	virtual bool queryInt(const std::string& sql, int& value, std::string& error) = 0;

	/**
	 * @brief 查询单个字符串列�?	 * @param sql 单标量查询语句�?	 * @param[out] value 输出列值�?	 * @param[out] error 失败说明�?	 * @return 成功 true�?	 */
	virtual bool queryString(const std::string& sql, std::string& value, std::string& error) = 0;
};

}
