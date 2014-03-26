#include "stdafx.h"
#include "ServerManager.h"
#include "ServerConnection.h"
#include "ServerIOCallback.h"

ServerIOCallback::ServerIOCallback(ServerConnection* conn)
: conn_(conn)
{
}

ServerIOCallback::~ServerIOCallback()
{
	ServerManager::get_instance().del(conn_);
	delete conn_;
}

bool ServerIOCallback::read_callback(char* data, int len)
{
	if (data == NULL || *data == 0 || len <= 0)
	{
		logger_warn("invalid data: %s, len: %d",
			data ? data : "null", len);
		return false;
	}

	// 处理服务端发来的命令

	acl::url_coder coder;
	coder.decode(data);

	const char* ptr = coder.get("client_count");
	if (ptr == NULL)
	{
		logger_warn("no client_count");
		return true;
	}

	unsigned int nconns = (unsigned int) atoi(ptr);
	conn_->set_nconns(nconns);

	// 尝试将服务端连接对象添加进服务端管理对象中
	ServerManager::get_instance().set(conn_);

	return true;
}

void ServerIOCallback::close_callback()
{
	delete this;
}

bool ServerIOCallback::timeout_callback()
{
	return true;
}
