#pragma once

//////////////////////////////////////////////////////////////////////////

// 纯虚类，子类须实现该类中的纯虚接口
class rpc_callback
{
public:
	rpc_callback() {}
	virtual ~rpc_callback() {}

	virtual void enable_ping() = 0;
};

//////////////////////////////////////////////////////////////////////////

class ping : public acl::rpc_request
{
public:
	ping(const char* filepath, rpc_callback* callback,
		int npkt = 10, int delay = 1, int timeout = 5);
protected:
	~ping() {}

	// 基类虚函数：子线程处理函数
	virtual void rpc_run();

	// 基类虚函数：主线程处理过程，收到子线程任务完成的消息
	virtual void rpc_onover();

	// 基类虚函数：主线程处理过程，收到子线程的通知消息
	virtual void rpc_wakeup(void* ctx);
private:
	acl::string filepath_;
	rpc_callback* callback_;
	int   npkt_;
	int   delay_;
	int   timeout_;
	std::list<acl::string> ip_list_;

	bool load_file();
	void ping_all();
};
