#pragma once
#include "acl_cpp/acl_cpp_define.hpp"
#include "acl_cpp/connpool/connect_pool.hpp"

namespace acl
{

class ACL_CPP_API redis_pool : public connect_pool
{
public:
	/**
	 * 构造函数
	 * @param addr {const char*} 服务端地址，格式：ip:port
	 * @param count {int} 连接池的最大连接数限制
	 * @param idx {size_t} 该连接池对象在集合中的下标位置(从 0 开始)
	 */
	redis_pool(const char* addr, int count, size_t idx = 0);
	virtual ~redis_pool();

	/**
	 * 设置网络连接超时时间及网络 IO 读写超时时间(秒)
	 * @param conn_timeout {int} 连接超时时间
	 * @param rw_timeout {int} 网络 IO 读写超时时间(秒)
	 * @return {redis_pool&}
	 */
	redis_pool& set_timeout(int conn_timeout = 30, int rw_timeout = 60);

protected:
	/**
	 * 基类纯虚函数: 调用此函数用来创建一个新的连接
	 * @return {connect_client*}
	 */
	virtual connect_client* create_connect();

private:
	int   conn_timeout_;
	int   rw_timeout_;
};

} // namespace acl
