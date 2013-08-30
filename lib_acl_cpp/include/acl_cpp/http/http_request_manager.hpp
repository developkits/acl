#pragma once
#include "acl_cpp/acl_cpp_define.hpp"
#include "acl_cpp/connpool/connect_manager.hpp"

namespace acl
{

/**
 * HTTP 客户端请求连接池管理类
 */
class http_request_manager : acl::connect_manager
{
public:
	http_request_manager();
	virtual ~http_request_manager();

protected:
	/**
	 * 基类纯虚函数，用来创建连接池对象
	 * @param addr {const char*} 服务器监听地址，格式：ip:port
	 * @param count {int} 连接池的大小限制
	 */
	virtual connect_pool* create_pool(const char* addr, int count);
};

} // namespace acl
