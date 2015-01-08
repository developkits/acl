#include "acl_stdafx.hpp"
#include "acl_cpp/stdlib/util.hpp"
#include "acl_cpp/stdlib/log.hpp"
#include "acl_cpp/stream/socket_stream.hpp"
#include "acl_cpp/redis/redis_result.hpp"
#include "acl_cpp/redis/redis_client.hpp"

namespace acl
{

redis_client::redis_client(const char* addr, int conn_timeout /* = 60 */,
	int rw_timeout /* = 30 */, bool retry /* = true */)
: conn_timeout_(conn_timeout)
, rw_timeout_(rw_timeout)
, retry_(retry)
, argv_size_(0)
, argv_(NULL)
, argv_lens_(NULL)
{
	addr_ = acl_mystrdup(addr);
	pool_ = NEW dbuf_pool();
}

redis_client::~redis_client()
{
	acl_myfree(addr_);
	delete pool_;
}

bool redis_client::open()
{
	if (conn_.opened())
		return true;
	if (conn_.open(addr_, conn_timeout_, rw_timeout_) == false)
	{
		logger_error("connect redis %s error: %s",
			addr_, last_serror());
		return false;
	}
	return true;
}

void redis_client::close()
{
	conn_.close();
}

redis_result* redis_client::get_error()
{
	buf_.clear();
	if (conn_.gets(buf_) == false)
		return NULL;

	redis_result* rr = NEW redis_result(pool_);
	rr->set_type(REDIS_RESULT_ERROR);
	rr->set_size(1);
	rr->put(buf_.c_str(), buf_.length());
	return rr;
}

redis_result* redis_client::get_status()
{
	buf_.clear();
	if (conn_.gets(buf_) == false)
		return NULL;

	redis_result* rr = NEW redis_result(pool_);
	rr->set_type(REDIS_RESULT_STATUS);
	rr->set_size(1);
	rr->put(buf_.c_str(), buf_.length());
	return rr;
}

redis_result* redis_client::get_integer()
{
	buf_.clear();
	if (conn_.gets(buf_) == false)
		return NULL;

	redis_result* rr = NEW redis_result(pool_);
	rr->set_type(REDIS_RESULT_INTEGER);
	rr->set_size(1);
	rr->put(buf_.c_str(), buf_.length());
	return rr;
}

redis_result* redis_client::get_string()
{
	buf_.clear();
	if (conn_.gets(buf_) == false)
		return NULL;
	redis_result* rr = NEW redis_result(pool_);
	rr->set_type(REDIS_RESULT_STRING);
	int len = atoi(buf_.c_str());
	if (len <= 0)
		return rr;

	// 将可能返回的大内存分成不连接的内存链存储

	char buf[8192];
	size_t size = len / (int) sizeof(buf);
	if (len % (int) sizeof(buf) != 0)
		size++;
	rr->set_size(size);
	int n;
	while (len > 0)
	{
		n = len > sizeof(buf) - 1 ? sizeof(buf) - 1 : len;
		if (conn_.read(buf, n) == -1)
		{
			delete rr;
			return NULL;
		}
		buf[n] = 0;
		rr->put(buf, n);
		len -= n;
	}
	
	return rr;
}

redis_result* redis_client::get_array()
{
	buf_.clear();
	if (conn_.gets(buf_) == false)
		return NULL;

	redis_result* rr = NEW redis_result(pool_);
	rr->set_type(REDIS_RESULT_ARRAY);
	int count = atoi(buf_.c_str());
	if (count <= 0)
		return rr;

	for (int i = 0; i < count; i++)
	{
		redis_result* child = get_object();
		if (child == NULL)
			return NULL;
		rr->put(child);
	}

	return rr;
}

redis_result* redis_client::get_object()
{
	char ch;
	if (conn_.read(ch) == false)
	{
		logger_error("read first char error");
		return NULL;
	}

	switch (ch)
	{
	case '-':	// ERROR
		return get_error();
	case '+':	// STATUS
		return get_status();
	case ':':	// INTEGER
		return get_integer();
	case '$':	// STRING
		return get_string();
	case '*':	// ARRAY
		return get_array();
	default:	// INVALID
		logger_error("invalid first char: %c, %d", ch, ch);
		return NULL;
	}
}

redis_result* redis_client::run(const string& request)
{
	bool retried = false;

	while (true)
	{
		if (!conn_.opened() && conn_.open(addr_, conn_timeout_,
			rw_timeout_) == false)
		{
			logger_error("connect server: %s error: %s",
				addr_, last_serror());
			return NULL;
		}

		if (conn_.write(request) == -1)
		{
			conn_.close();
			if (retry_ && !retried)
			{
				retried = true;
				continue;
			}
			logger_error("write to redis(%s) error: %s",
				addr_, last_serror());
			return NULL;
		}

		redis_result* rr = get_object();
		if (rr != NULL)
			return rr;
		conn_.close();

		if (!retry_ || retried)
			break;
	}

	return NULL;
}

void redis_client::argv_space(size_t n)
{
	if (argv_size_ >= n)
		return;
	argv_size_ = n;
	argv_ = (const char**) pool_->dbuf_alloc(n * sizeof(char*));
	argv_lens_ = (size_t*) pool_->dbuf_alloc(n * sizeof(size_t));
}

const string& redis_client::build_request(size_t argc, const char* argv[],
	size_t argv_lens[], string* buf /* = NULL */)
{
	if (buf == NULL)
	{
		buf = &request_;
		buf->clear();
	}

	buf->append("*%d\r\n", argc);
	for (size_t i = 0; i < argc; i++)
	{
		buf->append("$%lu\r\n", (unsigned long) argv_lens[i]);
		buf->append(argv[i], argv_lens[i]);
		buf->append("\r\n");
	}
	return *buf;
}

const string& redis_client::build_set(const char* cmd, const char* key,
	const std::map<string, string>& attrs, string* buf /* = NULL */)
{
	argc_ = 2 + attrs.size() * 2;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	std::map<string, string>::const_iterator cit = attrs.begin();
	for (; cit != attrs.end(); ++cit)
	{
		argv_[i] = cit->first.c_str();
		argv_lens_[i] = strlen(argv_[i]);
		i++;

		argv_[i] = cit->second.c_str();
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_set(const char* cmd, const char* key,
	const std::vector<char*>& names, const std::vector<char*>& values,
	string* buf /* = NULL */)
{
	if (names.size() != values.size())
	{
		logger_fatal("names's size: %lu, values's size: %lu",
			(unsigned long) names.size(),
			(unsigned long) values.size());
	}

	argc_ = 2 + names.size() * 2;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	size_t size = names.size();
	for (size_t j = 0; j < size; j++)
	{
		argv_[i] = names[j];
		argv_lens_[i] = strlen(argv_[i]);
		i++;

		argv_[i] = values[j];
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_set(const char* cmd, const char* key,
	const std::vector<string>& names, const std::vector<string>& values,
	string* buf /* = NULL */)
{
	if (names.size() != values.size())
	{
		logger_fatal("names's size: %lu, values's size: %lu",
			(unsigned long) names.size(),
			(unsigned long) values.size());
	}

	argc_ = 2 + names.size() * 2;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	size_t size = names.size();
	for (size_t j = 0; j < size; j++)
	{
		argv_[i] = names[j].c_str();
		argv_lens_[i] = strlen(argv_[i]);
		i++;

		argv_[i] = values[j].c_str();
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_set(const char* cmd, const char* key,
	const char* names[], const char* values[], size_t argc,
	string* buf /* = NULL */)
{
	argc_ = 2 + argc * 2;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	for (size_t j = 0; j < argc; j++)
	{
		argv_[i] = names[j];
		argv_lens_[i] = strlen(argv_[i]);
		i++;

		argv_[i] = values[j];
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_set(const char* cmd, const char* key,
	const char* names[], size_t names_len[],
	const char* values[], size_t values_len[],
	size_t argc, string* buf /* = NULL */)
{
	argc_ = 2 + argc * 2;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	for (size_t j = 0; j < argc; j++)
	{
		argv_[i] = names[j];
		argv_lens_[i] = names_len[i];
		i++;

		argv_[i] = values[j];
		argv_lens_[i] = values_len[i];
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_get(const char* cmd, const char* key,
	const std::vector<string>& names, string* buf /* = NULL */)
{
	size_t argc = names.size();
	argc_ = 2 + argc;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	for (size_t j = 0; j < argc; j++)
	{
		argv_[i] = names[i].c_str();
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_get(const char* cmd, const char* key,
	const std::vector<char*>& names, string* buf /* = NULL */)
{
	size_t argc = names.size();
	argc_ = 2 + argc;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	for (size_t j = 0; j < argc; j++)
	{
		argv_[i] = names[i];
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_get(const char* cmd, const char* key,
	const std::vector<const char*>& names, string* buf /* = NULL */)
{
	size_t argc = names.size();
	argc_ = 2 + argc;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	for (size_t j = 0; j < argc; j++)
	{
		argv_[i] = names[i];
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_get(const char* cmd, const char* key,
	const char* names[], size_t argc, string* buf /* = NULL */)
{
	argc_ = 2 + argc;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	for (size_t j = 0; j < argc; j++)
	{
		argv_[i] = names[i];
		argv_lens_[i] = strlen(argv_[i]);
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

const string& redis_client::build_get(const char* cmd, const char* key,
	const char* names[], const size_t lens[],
	size_t argc, string* buf /* = NULL */)
{
	argc_ = 2 + argc;
	argv_space(argc_);

	size_t i = 0;
	argv_[i] = cmd;
	argv_lens_[i] = strlen(cmd);
	i++;

	argv_[i] = key;
	argv_lens_[i] = strlen(key);
	i++;

	for (size_t j = 0; j < argc; j++)
	{
		argv_[i] = names[i];
		argv_lens_[i] = lens[i];
		i++;
	}

	return build_request(argc_, argv_, argv_lens_, buf);
}

bool redis_client::delete_keys(const std::list<string>& keys)
{
	return true;
}

bool redis_client::delete_keys(const char* key1, ...)
{
	return true;
}

} // end namespace acl
