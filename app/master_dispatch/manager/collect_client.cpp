#include "stdafx.h"
#include "message.h"
#include "message_manager.h"
#include "collect_client.h"

collect_client::collect_client(message_manager& manager, const char* server)
: manager_(manager)
, server_(server)
{

}

collect_client::~collect_client()
{

}

void* collect_client::run()
{
	// 采用 url 编码方式：
	// type=xxx

	// 创建 HTTP 请求头
	acl::http_request req(server_);
	req.request_header()
		.set_url("/?type=xml")
		.set_keep_alive(false)
		.set_method(acl::HTTP_METHOD_GET);

	req.set_timeout(var_cfg_conn_timeout, var_cfg_rw_timeout);

	// 发送 HTTP 请求头 & 读取 HTTP 响应头
	if (req.request(NULL, 0) == false)
	{
		logger_error("request to server: %s", server_.c_str());

		message* msg = new message(server_, NULL);
		manager_.put(msg);

		return NULL;
	}

	acl::http_client* conn = req.get_client();
	//int  http_status = conn->response_status();

	acl::string buf(1024);
	message* msg = new message(server_, NULL);
	while (true)
	{
		buf.clear();
		int   ret = req.read_body(buf);
		if (ret <= 0)
			break;
		msg->add(buf.c_str(), buf.length());
	}

	manager_.put(msg);
	return NULL;
}
