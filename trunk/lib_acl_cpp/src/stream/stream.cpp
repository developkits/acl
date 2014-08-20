#include "acl_stdafx.hpp"
#include "acl_cpp/stdlib/log.hpp"
#include "acl_cpp/stdlib/string.hpp"
#include "acl_cpp/stream/stream.hpp"

namespace acl {

stream::stream(void)
: hook_(NULL)
, stream_(NULL)
, eof_(true)
, opened_(false)
, obj_id_(-1)
, default_ctx_(NULL)
{
}

stream::~stream(void)
{
	if (hook_)
		hook_->destroy();
	if (stream_)
		acl_vstream_free(stream_);
}

bool stream::eof(void) const
{
	return eof_;
}

void stream::clear_eof(void)
{
	eof_ = false;
}

bool stream::opened(void) const
{
	return opened_;
}

ACL_VSTREAM* stream::get_vstream() const
{
	return stream_;
}

void stream::set_rw_timeout(int n)
{
	if (stream_)
		stream_->rw_timeout = n;
}

int stream::get_rw_timeout() const
{
	if (stream_ == NULL)
		return -1;
	return stream_->rw_timeout;
}

ACL_VSTREAM* stream::unbind()
{
	eof_ = true;
	opened_ = false;
	ACL_VSTREAM* vstream = stream_;
	stream_ = NULL;
	return vstream;
}

bool stream::close()
{
	if (opened_ == false)
		return false;
	if (stream_ == NULL)
		return true;

	eof_ = true;
	opened_ = false;

	if (hook_)
		(void) hook_->on_close();

	int ret = acl_vstream_close(stream_);
	stream_ = NULL;

	return ret == 0 ? true : false;
}

void stream::open_stream(void)
{
	if (stream_ != NULL)
		return;
	stream_ = acl_vstream_fdopen(ACL_SOCKET_INVALID, O_RDWR,
		8192, 0, ACL_VSTREAM_TYPE_SOCK);
}

void stream::reopen_stream(void)
{
	if (stream_)
		acl_vstream_free(stream_);
	open_stream();
}

bool stream::set_ctx(void* ctx, const char* key /* = NULL */,
	bool replace /* = true */)
{
	if (key == NULL || *key == 0)
	{
		if (!replace && default_ctx_ != NULL)
			return false;
		default_ctx_ = ctx;
		return true;
	}
	if (replace)
	{
		ctx_table_[key] = ctx;
		return true;
	}

	std::map<string, void*>::const_iterator cit = ctx_table_.find(key);
	if (cit != ctx_table_.end())
		return false;
	ctx_table_[key] = ctx;
	return true;
}

void* stream::get_ctx(const char* key /* = NULL */) const
{
	if (key == NULL || *key == 0)
		return default_ctx_;
	std::map<string, void*>::const_iterator it = ctx_table_.find(key);
	if (it != ctx_table_.end())
		return it->second;
	else
		return NULL;
}

void* stream::del_ctx(const char* key /* = NULL */)
{
	if (key == NULL || *key == 0)
	{
		if (default_ctx_ == NULL)
			return NULL;
		void* ctx = default_ctx_;
		default_ctx_ = NULL;
		return ctx;
	}

	std::map<string, void*>::iterator it = ctx_table_.find(key);
	if (it == ctx_table_.end())
		return NULL;
	void *ctx = it->second;
	ctx_table_.erase(it);
	return ctx;
}

stream_hook* stream::get_hook() const
{
	return hook_;
}

stream_hook* stream::remove_hook()
{
	stream_hook* hook = hook_;
	hook_ = NULL;

	if (stream_->type == ACL_VSTREAM_TYPE_FILE)
	{
		stream_->fread_fn  = acl_file_read;
		stream_->fwrite_fn = acl_file_write;
		stream_->fwritev_fn = acl_file_writev;
		stream_->fclose_fn = acl_file_close;
	}
	else
	{
		stream_->read_fn  = acl_socket_read;
		stream_->write_fn = acl_socket_write;
		stream_->writev_fn = acl_socket_writev;
		stream_->close_fn = acl_socket_close;
	}

	return hook;
}

stream_hook* stream::setup_hook(stream_hook* hook)
{
	if (stream_ == NULL)
	{
		logger_error("stream_ null");
		return NULL;
	}

	stream_hook* old_hook = hook_;

	if (stream_->type == ACL_VSTREAM_TYPE_FILE)
	{
		ACL_FSTREAM_RD_FN read_fn = stream_->fread_fn;
		ACL_FSTREAM_WR_FN write_fn = stream_->fwrite_fn;
		void* ctx = stream_->context;

		stream_->fread_fn = fread_hook;
		stream_->fwrite_fn = fsend_hook;
		obj_id_  = acl_vstream_add_object(stream_, this);

		if (hook->open(this) == false)
		{
			// 如果打开失败，则恢复

			stream_->fread_fn = read_fn;
			stream_->fwrite_fn = write_fn;
			stream_->context = ctx;
			return hook;
		}
	}
	else
	{
		ACL_VSTREAM_RD_FN read_fn = stream_->read_fn;
		ACL_VSTREAM_WR_FN write_fn = stream_->write_fn;
		void* ctx = stream_->context;

		stream_->read_fn = read_hook;
		stream_->write_fn = send_hook;
		obj_id_  = acl_vstream_add_object(stream_, this);

		acl_tcp_set_nodelay(ACL_VSTREAM_SOCK(stream_));

		if (hook->open(this) == false)
		{
			// 如果打开失败，则恢复

			stream_->read_fn = read_fn;
			stream_->write_fn = write_fn;
			stream_->context = ctx;
			return hook;
		}
	}

	hook_ = hook;
	return old_hook;
}

int stream::read_hook(ACL_SOCKET, void *buf, size_t len, int,
	ACL_VSTREAM* vs, void *)
{
	stream* s = acl_vstream_get_obj(vs, obj_id_);
	acl_assert(s);

	if (s->hook_ == NULL)
	{
		logger_error("hook_ null");
		return -1;
	}
	return s->hook_->read(buf, len);
}

int stream::send_hook(ACL_SOCKET, const void *buf, size_t len, int,
	ACL_VSTREAM* vs, void *)
{
	stream* s = acl_vstream_get_obj(vs, obj_id_);
	acl_assert(s);

	if (s->hook_ == NULL)
	{
		logger_error("hook_ null");
		return -1;
	}
	return s->hook_->send(buf, len);
}

int stream::fread_hook(ACL_FILE_HANDLE, void *buf, size_t len, int,
	ACL_VSTREAM* vs, void *)
{
	stream* s = acl_vstream_get_obj(vs, obj_id_);
	acl_assert(s);

	if (s->hook_ == NULL)
	{
		logger_error("hook_ null");
		return -1;
	}
	return s->hook_->read(buf, len);
}

int stream::fsend_hook(ACL_FILE_HANDLE, const void *buf, size_t len, int,
	ACL_VSTREAM* vs, void *)
{
	stream* s = acl_vstream_get_obj(vs, obj_id_);
	acl_assert(s);

	if (s->hook_ == NULL)
	{
		logger_error("hook_ null");
		return -1;
	}
	return s->hook_->send(buf, len);
}

} // namespace acl
