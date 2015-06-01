#include "acl_stdafx.hpp"
#include "acl_cpp/stdlib/log.hpp"
#include "acl_cpp/memcache/memcache.hpp"
#include "acl_cpp/session/memcache_session.hpp"

namespace acl
{

memcache_session::memcache_session(const char* cache_addr,
	int conn_timeout /* = 180 */, int rw_timeout /* = 300 */,
	const char* prefix /* = NULL */, time_t ttl /* = 0 */, 
	const char* sid /* = NULL */, bool encode_key /* = true */)
: session(ttl, sid)
, auto_free_(true)
{
	acl_assert(cache_addr && *cache_addr);
	cache_ = NEW memcache(cache_addr, conn_timeout, rw_timeout);
	(*cache_).set_prefix(prefix && *prefix ? prefix : "_")
		.encode_key(encode_key)
		.auto_retry(true);
}

memcache_session::memcache_session(memcache* cache, bool auto_free /* = false */,
	time_t ttl /* = 0 */, const char* sid /* = NULL */)
: session(ttl, sid)
, cache_(cache)
, auto_free_(auto_free)
{

}

memcache_session::~memcache_session()
{
	if (auto_free_)
		delete cache_;
}

bool memcache_session::get_attrs(const char* sid,
	std::map<string, VBUF*>& attrs)
{
	// 清空原有数据
	attrs_clear(attrs);

	string buf;
	if (cache_->get(sid, buf) == false)
		return false;

	// 反序列化
	deserialize(buf, attrs);
	return true;
}

bool memcache_session::set_attrs(const char* sid,
	std::map<string, VBUF*>& attrs, time_t ttl)
{
	string buf;
	serialize(attrs, buf);  // 序列化数据
	attrs_clear(attrs);  // 清除属性集合数据
	return cache_->set(sid, buf.c_str(), buf.length(), ttl);
}

bool memcache_session::del_key(const char* sid)
{
	return cache_->del(sid);
}

bool memcache_session::set_timeout(const char* sid, time_t ttl)
{
	return cache_->set(sid, ttl);
}

} // namespace acl
