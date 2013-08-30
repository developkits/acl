#include "acl_stdafx.hpp"
#include "acl_cpp/http/http_request_pool.hpp"
#include "acl_cpp/http/http_request_manager.hpp"

namespace acl
{

http_request_manager::http_request_manager()
{

}

http_request_manager::~http_request_manager()
{

}

connect_pool* http_request_manager::create_pool(const char* addr, int count)
{
	http_request_pool* conns = NEW http_request_pool(addr, count);

	return conns;
}

}  // namespace acl
