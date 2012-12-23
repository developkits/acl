#include "acl_stdafx.hpp"
#include "string.hpp"
#include "ostream.hpp"
#include "socket_stream.hpp"
#include "http_header.hpp"
#include "HttpServlet.hpp"
#include "HttpServletResponse.hpp"

namespace acl
{

HttpServletResponse::HttpServletResponse(socket_stream& stream)
: stream_(stream)
{
	header_ = NEW http_header();
	header_->set_request_mode(false);
	charset_[0] = 0;
	snprintf(content_type_, sizeof(content_type_), "text/html");
}

HttpServletResponse::~HttpServletResponse(void)
{
	delete header_;
}

HttpServletResponse& HttpServletResponse::setRedirect(const char* location, int status /* = 302 */)
{
	header_->add_entry("Location", location);
	header_->set_status(status);
	return *this;
}

HttpServletResponse& HttpServletResponse::setCharacterEncoding(const char* charset)
{
	snprintf(charset_, sizeof(charset_), "%s", charset);
	return *this;
}

HttpServletResponse& HttpServletResponse::setKeepAlive(bool on)
{
	header_->set_keep_alive(on);
	return *this;
}

HttpServletResponse& HttpServletResponse::setContentLength(acl_int64 n)
{
	header_->set_content_length(n);
	return *this;
}

HttpServletResponse& HttpServletResponse::setContentType(const char* value)
{
	snprintf(content_type_, sizeof(content_type_), "%s", value);
	return *this;
}

HttpServletResponse& HttpServletResponse::setDateHeader(const char* name, time_t value)
{
	char buf[256];
	header_->date_format(buf, sizeof(buf), value);
	header_->add_entry(name, buf);
	return *this;
}

HttpServletResponse& HttpServletResponse::setHeader(const char* name, int value)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", value);
	header_->add_entry(name, buf);
	return *this;
}

HttpServletResponse& HttpServletResponse::setHeader(const char* name, const char* value)
{
	header_->add_entry(name, value);
	return *this;
}

HttpServletResponse& HttpServletResponse::setStatus(int status)
{
	header_->set_status(status);
	return *this;
}

HttpServletResponse& HttpServletResponse::setCgiMode(bool on)
{
	header_->set_cgi_mode(on);
	return *this;
}

HttpServletResponse& HttpServletResponse::addCookie(HttpCookie* cookie)
{
	header_->add_cookie(cookie);
	return *this;
}

HttpServletResponse& HttpServletResponse::addCookie(const char* name, const char* value,
	const char* domain /* = NULL */, const char* path /* = NULL */,
	time_t expires /* = 0 */)
{
	acl_assert(name && *name && value);
	header_->add_cookie(name, value, domain, path, expires);
	return *this;
}

http_header& HttpServletResponse::getHttpHeader(void) const
{
	return *header_;
}

bool HttpServletResponse::sendHeader(void)
{
	acl_assert(header_->is_request() == false);
	string buf;
	if (charset_[0] != 0)
		buf.format("%s; charset=%s", content_type_, charset_);
	else
		buf.format("%s", content_type_);
	header_->set_content_type(buf.c_str());

	buf.clear();
	header_->build_response(buf);
	return getOutputStream().write(buf) == -1 ? false : true;
}

void HttpServletResponse::encodeUrl(string& out, const char* url)
{
	out.clear();
	out.url_encode(url);
}

ostream& HttpServletResponse::getOutputStream(void) const
{
	return stream_;
}

} // namespace acl
