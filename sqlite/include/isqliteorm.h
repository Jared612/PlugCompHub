/**
 * @file iSqliteOrm.h
 * @brief 鍩轰簬 SQLite 鐨勯敭鍊艰〃 ORM 鎶借薄锛堝崟琛?KV锛夈€? * @details 鍏稿瀷瀹炵幇浣跨敤鍥哄畾 schema 瀛?key/value锛涢€傚悎杞婚噺閰嶇疆鎴栫紦瀛橈紝闈為€氱敤鍏崇郴 ORM銆? */
#pragma once

#include <string>

namespace pch {

/**
 * @brief 閿€煎瓨鍌ㄦ帴鍙ｏ紙琛ㄧ骇灏佽锛夈€? * @note `open` 鎴愬姛鍚庡啀璋冪敤鍏跺畠鎺ュ彛锛涘苟鍙戠敱璋冪敤鏂瑰崗璋冦€? */
class ISqliteOrm
{
public:
	virtual ~ISqliteOrm() = default;

	/**
	 * @brief 鎵撳紑鏁版嵁搴撳苟鍑嗗 KV 琛ㄧ粨鏋勩€?	 * @param dbPath 鏁版嵁搴撴枃浠惰矾寰勩€?	 * @param[out] error 澶辫触鍘熷洜銆?	 * @return 鎴愬姛 true銆?	 */
	virtual bool open(const std::string& dbPath, std::string& error) = 0;

	/** @brief 鍏抽棴鏁版嵁搴撹繛鎺ャ€?*/
	virtual void close() = 0;

	/** @return 鏄惁澶勪簬鎵撳紑鐘舵€併€?*/
	virtual bool isOpen() const = 0;

	/**
	 * @brief 鍐欏叆鎴栨洿鏂颁竴鏉¤褰曘€?	 * @param key 閿€?	 * @param value 鍊笺€?	 * @param[out] error 澶辫触鍘熷洜銆?	 * @return 鎴愬姛 true銆?	 */
	virtual bool put(const std::string& key, const std::string& value, std::string& error) = 0;

	/**
	 * @brief 鎸夐敭璇诲彇鍊笺€?	 * @param key 閿€?	 * @param[out] value 杈撳嚭鍊硷紱鏈壘鍒版椂鐢卞疄鐜板喅瀹?`value` 鏄惁娓呯┖銆?	 * @param[out] error 澶辫触鎴栥€屾湭鎵惧埌銆嶈鏄庛€?	 * @return 鎴愬姛 true銆?	 */
	virtual bool get(const std::string& key, std::string& value, std::string& error) = 0;

	/**
	 * @brief 鍒犻櫎鎸囧畾閿€?	 * @param[out] error 澶辫触鍘熷洜銆?	 * @return 鎴愬姛 true锛堝垹闄?0 琛屼篃鍙兘瑙嗕负鎴愬姛锛屼互瀹炵幇涓哄噯锛夈€?	 */
	virtual bool erase(const std::string& key, std::string& error) = 0;

	/**
	 * @brief 杩斿洖褰撳墠 KV 鏉℃暟銆?	 * @param[out] error 澶辫触鍘熷洜銆?	 * @return 琛屾暟锛涘け璐ヨ繑鍥炶礋鍊硷紙鑻ュ疄鐜版棤娉曞尯鍒嗭紝鍙煡 `error`锛夈€?	 */
	virtual int count(std::string& error) = 0;
};

}
