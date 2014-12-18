#include "acl_stdafx.hpp"
#include "acl_cpp/stdlib/snprintf.hpp"

namespace acl
{

#ifdef WIN32
#include <stdarg.h>

# ifdef __STDC_WANT_SECURE_LIB__

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	int ret = acl::vsnprintf(buf, size, fmt, ap);  // 调用 acl::vsnprintf
	va_end(ap);
	return ret;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	if (size == 0)
	{
		buf[0] = 0;
		return -1;
	}

	int ret = ::_vsnprintf_s(buf, size, _TRUNCATE, fmt, ap);
	if (ret < 0)
		return -1;
	else
		return ret;
}

# else

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	int ret = acl::vsnprintf(buf, size, fmt, ap);  // 调用 acl::vsnprintf
	va_end(ap);
	return ret;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	if (size == 0)
	{
		buf[0] = 0;
		return -1;
	}

	int   ret = ::_vsnprintf(buf, size, fmt, ap);
	if (ret < 0 || ret >= (int) size)
	{
		buf[size - 1] = 0;
		return -1;
	}
	else
		return ret;
}

# endif // __STDC_WANT_SECURE_LIB__

#else

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	int ret = vsnprintf(buf, size, fmt, ap);  // 调用 acl::vsnprintf
	va_end(ap);
	return ret;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	return ::vsnprintf(buf, size, fmt, ap);
}

#endif // !WIN32

} // namespace acl
