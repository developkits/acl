#pragma once
#include <vector>

class ServerConnection;

/**
 * 服务端连接对象管理器
 */
class ServerManager : public acl::singleton <ServerManager>
{
public:
	ServerManager() {}
	~ServerManager() {}

	/**
	 * 添加新的服务端连接对象
	 * @param conn {ServerConnection*}
	 */
	void set(ServerConnection* conn);

	/**
	 * 删除某个指定的服务端连接对象
	 */
	void del(ServerConnection* conn);

	/**
	 * 取出服务端连接对象中负载最低的一个连接对象
	 * @return {ServerConnection*} 返回 NULL 表示没有可用的服务对象
	 */
	ServerConnection* min();

	/**
	 * 获得所有的服务端连接对象的个数
	 * @return {size_t}
	 */
	size_t length() const
	{
		return conns_.size();
	}

	/**
	 * 将服务器连接对象集合转换为 JSON 对象
	 */
	void buildJson();

	void toString(acl::string& buf);

private:
	std::vector<ServerConnection*> conns_;
	acl::json json_;
	acl::locker lock_;
};
