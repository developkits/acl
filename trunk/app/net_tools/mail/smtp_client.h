#pragma once

//////////////////////////////////////////////////////////////////////////

struct SMTP_METER
{
	double smtp_nslookup_elapsed;
	double smtp_connect_elapsed;
	double smtp_envelope_eplased;
	double smtp_auth_elapsed;
	double smtp_data_elapsed;
	double smtp_total_elapsed;

	double pop3_nslookup_elapsed;
	double pop3_connect_elapsed;
	double pop3_auth_elapsed;
	double pop3_list_elapsed;
	double pop3_total_elapsed;
};

//////////////////////////////////////////////////////////////////////////

class smtp_callback
{
public:
	smtp_callback() {}
	virtual ~smtp_callback() {}

	virtual void smtp_finish(const char* dbpath) = 0;
	virtual void smtp_report(const char* msg, size_t total,
		size_t curr, const SMTP_METER& meter) = 0;
};

//////////////////////////////////////////////////////////////////////////

class smtp_client : public acl::rpc_request
{
public:
	smtp_client();

	smtp_client& set_callback(smtp_callback*);

	smtp_client& set_account(const char*);
	smtp_client& set_passwd(const char*);
	smtp_client& set_conn_timeout(int);
	smtp_client& set_rw_timeout(int);

	smtp_client& set_smtp(const char*, int);
	smtp_client& set_from(const char*);
	smtp_client& add_to(const char*);
	smtp_client& set_subject(const char*);
	smtp_client& add_file(const char*);

	smtp_client& set_pop3(const char*, int);
protected:
	~smtp_client();

	// 基类虚函数：子线程处理函数
	virtual void rpc_run();

	// 基类虚函数：主线程处理过程，收到子线程任务完成的消息
	virtual void rpc_onover();

	// 基类虚函数：主线程处理过程，收到子线程的通知消息
	virtual void rpc_wakeup(void* ctx);
private:
	SMTP_METER meter_;
private:
	smtp_callback* callback_;
	int connect_timeout_;
	int rw_timeout_;
	acl::string auth_account_;
	acl::string auth_passwd_;

	acl::string smtp_ip_;
	acl::string smtp_addr_;
	int smtp_port_;
	acl::string mail_from_;
	std::list<acl::string> recipients_;
	acl::string subject_;
	std::vector<acl::string> files_;

	acl::string pop3_ip_;
	acl::string pop3_addr_;
	int pop3_port_;

	acl::string mailpath_;
private:
	void test_smtp();
	void test_pop3();
};