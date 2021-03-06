#ifdef ECOS
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/bsdtypes.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include "types.h"
#include "patch_for_ecos.h"

#ifndef MAX
#define	MAX(a, b) 		((a) < (b) ? (b) : (a))
#endif

#ifndef howmany
#define        howmany(x, y)   (((x)+((y)-1))/(y))
#endif

static int	inet_pton4 (const char *src, uint8_t *dst);
static int	inet_pton6 (const char *src, uint8_t *dst);

__externC ssize_t  read(int, void *, size_t);
__externC ssize_t  write(int, const void *, size_t);
__externC off_t lseek( int fd, off_t pos, int whence );

/*
 *
 * The definitions we might miss.
 *
 */
#ifndef NS_INT16SZ
#define	NS_INT16SZ	2
#endif

#ifndef NS_IN6ADDRSZ
#define NS_IN6ADDRSZ 16
#endif

#ifndef NS_INADDRSZ
#define NS_INADDRSZ 4
#endif

#ifndef IN6ADDRSZ
#define IN6ADDRSZ   16   /* IPv6 T_AAAA */
#endif

#ifndef INT16SZ
#define INT16SZ     2    /* for systems without 16-bit ints */
#endif


/* XXX: There is no FPOS_MAX.  This assumes fpos_t is an off_t. */
#define	FPOS_MAX	SSIZE_MAX

#define FILE_NAME_LEN_MAX 32
#define FILE_NAME_PREFIX "/dev/tmp/file"

#define BUF_SIZE 8192

// #define EOVERFLOW   ETOOMANYREFS

// struct memstream
// {
// 	char **bufp;
// 	size_t *sizep;
// 	ssize_t len;
// 	fpos_t offset;
// };

// static int
// memstream_grow(struct memstream *ms, fpos_t newoff)
// {
// 	char *buf;
// 	ssize_t newsize;

// 	if (newoff < 0 || newoff >= SSIZE_MAX)
// 		newsize = SSIZE_MAX - 1;
// 	else
// 		newsize = newoff;
// 	if (newsize > ms->len)
// 	{
// 		buf = realloc(*ms->bufp, newsize + 1);
// 		if (buf != NULL)
// 		{
// #ifdef DEBUG
// 			fprintf(stderr, "MS: %p growing from %zd to %zd\n",
// 			        ms, ms->len, newsize);
// #endif
// 			memset(buf + ms->len + 1, 0, newsize - ms->len);
// 			*ms->bufp = buf;
// 			ms->len = newsize;
// 			return (1);
// 		}
// 		return (0);
// 	}
// 	return (1);
// }

// static void
// memstream_update(struct memstream *ms)
// {

// 	assert(ms->len >= 0 && ms->offset >= 0);
// 	*ms->sizep = ms->len < ms->offset ? ms->len : ms->offset;
// }

// static int
// memstream_write(void *cookie, const char *buf, int len)
// {
// 	struct memstream *ms;
// 	ssize_t tocopy;

// 	ms = cookie;
// 	if (!memstream_grow(ms, ms->offset + len))
// 		return (-1);
// 	tocopy = ms->len - ms->offset;
// 	if (len < tocopy)
// 		tocopy = len;
// 	memcpy(*ms->bufp + ms->offset, buf, tocopy);
// 	ms->offset += tocopy;
// 	memstream_update(ms);
// #ifdef DEBUG
// 	fprintf(stderr, "MS: write(%p, %d) = %zd\n", ms, len, tocopy);
// #endif
// 	return (tocopy);
// }

// static fpos_t
// memstream_seek(void *cookie, fpos_t pos, int whence)
// {
// 	struct memstream *ms;
// #ifdef DEBUG
// 	fpos_t old;
// #endif

// 	ms = cookie;
// #ifdef DEBUG
// 	old = ms->offset;
// #endif
// 	switch (whence)
// 	{
// 	case SEEK_SET:
// 		/* _fseeko() checks for negative offsets. */
// 		assert(pos >= 0);
// 		ms->offset = pos;
// 		break;
// 	case SEEK_CUR:
// 		/* This is only called by _ftello(). */
// 		assert(pos == 0);
// 		break;
// 	case SEEK_END:
// 		if (pos < 0)
// 		{
// 			if (pos + ms->len < 0)
// 			{
// #ifdef DEBUG
// 				fprintf(stderr,
// 				        "MS: bad SEEK_END: pos %jd, len %zd\n",
// 				        (intmax_t)pos, ms->len);
// #endif
// 				errno = EINVAL;
// 				return (-1);
// 			}
// 		}
// 		else
// 		{
// 			if (FPOS_MAX - ms->len < pos)
// 			{
// #ifdef DEBUG
// 				fprintf(stderr,
// 				        "MS: bad SEEK_END: pos %jd, len %zd\n",
// 				        (intmax_t)pos, ms->len);
// #endif
// 				errno = EOVERFLOW;
// 				return (-1);
// 			}
// 		}
// 		ms->offset = ms->len + pos;
// 		break;
// 	}
// 	memstream_update(ms);
// #ifdef DEBUG
// 	fprintf(stderr, "MS: seek(%p, %jd, %d) %jd -> %jd\n", ms, (intmax_t)pos,
// 	        whence, (intmax_t)old, (intmax_t)ms->offset);
// #endif
// 	return (ms->offset);
// }

// static int
// memstream_close(void *cookie)
// {

// 	free(cookie);
// 	return (0);
// }

// FILE *
// open_memstream(char **bufp, size_t *sizep)
// {
// 	struct memstream *ms;
// 	int save_errno;
// 	FILE *fp;

// 	if (bufp == NULL || sizep == NULL)
// 	{
// 		errno = EINVAL;
// 		return (NULL);
// 	}
// 	*bufp = calloc(1, 1);
// 	if (*bufp == NULL)
// 		return (NULL);
// 	ms = malloc(sizeof(*ms));
// 	if (ms == NULL)
// 	{
// 		save_errno = errno;
// 		free(*bufp);
// 		*bufp = NULL;
// 		errno = save_errno;
// 		return (NULL);
// 	}
// 	ms->bufp = bufp;
// 	ms->sizep = sizep;
// 	ms->len = 0;
// 	ms->offset = 0;
// 	memstream_update(ms);
// 	fp = funopen(ms, NULL, memstream_write, memstream_seek,
// 	             memstream_close);
// 	if (fp == NULL)
// 	{
// 		save_errno = errno;
// 		free(ms);
// 		free(*bufp);
// 		*bufp = NULL;
// 		errno = save_errno;
// 		return (NULL);
// 	}
// 	fwide(fp, -1);
// 	return (fp);
// }

ssize_t
sendfile_PATCH(int out_fd, int in_fd, off_t *offset, size_t count)
{
	off_t orig;
	char buf[BUF_SIZE];
	size_t toRead, numRead, numSent, totSent;

	if (offset != NULL)
	{

		/* Save current file offset and set offset to value in '*offset' */

		orig = lseek(in_fd, 0, SEEK_CUR);
		if (orig == -1)
			return -1;
		if (lseek(in_fd, *offset, SEEK_SET) == -1)
			return -1;
	}

	totSent = 0;

	while (count > 0)
	{
		toRead = min(BUF_SIZE, count);

		numRead = read(in_fd, buf, toRead);
		if (numRead == -1)
			return -1;
		if (numRead == 0)
			break;                      /* EOF */

		numSent = write(out_fd, buf, numRead);
		if (numSent == -1)
			return -1;
		if (numSent == 0)               /* Should never happen */
			printf("write() transferred 0 bytes\n");
		//fatal("sendfile: write() transferred 0 bytes");

		count -= numSent;
		totSent += numSent;
	}

	if (offset != NULL)
	{

		/* Return updated file offset in '*offset', and reset the file offset
		   to the value it had when we were called. */

		*offset = lseek(in_fd, 0, SEEK_CUR);
		if (*offset == -1)
			return -1;
		if (lseek(in_fd, orig, SEEK_SET) == -1)
			return -1;
	}

	return totSent;
}

int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	nfds_t i;
	int saved_errno, ret, fd, maxfd = 0;
	fd_set *readfds = NULL, *writefds = NULL, *exceptfds = NULL;
	size_t nmemb;
	struct timeval tv, *tvp = NULL;

	for (i = 0; i < nfds; i++)
	{
		fd = fds[i].fd;
		if (fd >= FD_SETSIZE)
		{
			errno = EINVAL;
			return -1;
		}
		maxfd = MAX(maxfd, fd);
	}

	nmemb = howmany(maxfd + 1 , __NFDBITS);
	if ((readfds = calloc(nmemb, sizeof(fd_mask))) == NULL ||
	        (writefds = calloc(nmemb, sizeof(fd_mask))) == NULL ||
	        (exceptfds = calloc(nmemb, sizeof(fd_mask))) == NULL)
	{
		saved_errno = ENOMEM;
		ret = -1;
		goto out;
	}

	/* populate event bit vectors for the events we're interested in */
	for (i = 0; i < nfds; i++)
	{
		fd = fds[i].fd;
		if (fd == -1)
			continue;
		if (fds[i].events & POLLIN)
		{
			FD_SET(fd, readfds);
			FD_SET(fd, exceptfds);
		}
		if (fds[i].events & POLLOUT)
		{
			FD_SET(fd, writefds);
			FD_SET(fd, exceptfds);
		}
	}

	/* poll timeout is msec, select is timeval (sec + usec) */
	if (timeout >= 0)
	{
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tvp = &tv;
	}

	ret = select(maxfd + 1, readfds, writefds, exceptfds, tvp);
	saved_errno = errno;

	/* scan through select results and set poll() flags */
	for (i = 0; i < nfds; i++)
	{
		fd = fds[i].fd;
		fds[i].revents = 0;
		if (fd == -1)
			continue;
		if (FD_ISSET(fd, readfds))
		{
			fds[i].revents |= POLLIN;
		}
		if (FD_ISSET(fd, writefds))
		{
			fds[i].revents |= POLLOUT;
		}
		if (FD_ISSET(fd, exceptfds))
		{
			fds[i].revents |= POLLERR;
		}
	}

out:
	free(readfds);
	free(writefds);
	free(exceptfds);
	if (ret == -1)
		errno = saved_errno;
	return ret;
}



/* int
 * inet_pton(af, src, dst)
 *	convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * return:
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *	Paul Vixie, 1996.
 */

char *get_available_file_name(void)
{
	static int file_index = 0;
	static char file_name[FILE_NAME_LEN_MAX] = {0};
	memset(file_name, 0, FILE_NAME_LEN_MAX);
	snprintf(file_name, (int)(3 + strlen(FILE_NAME_PREFIX)), "%s%d", FILE_NAME_PREFIX, file_index++);
	return file_name;
}


int
inet_pton_PATCH(af, src, dst)
int af;
const char *src;
void *dst;
{
	switch (af)
	{
	case AF_INET:
		return (inet_pton4(src, dst));
	case AF_INET6:
		return (inet_pton6(src, dst));
	default:
#ifdef EAFNOSUPPORT
		errno = EAFNOSUPPORT;
#else
		errno = ENOSYS;
#endif
		return (-1);
	}
	/* NOTREACHED */
}

/* int
 * inet_pton4(src, dst)
 *	like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *	1 if `src' is a valid dotted quad, else 0.
 * notice:
 *	does not touch `dst' unless it's returning 1.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton4(src, dst)
const char *src;
uint8_t *dst;
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	uint8_t tmp[NS_INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0')
	{
		const char *pch;

		if ((pch = strchr(digits, ch)) != NULL)
		{
			uint32_t new = *tp * 10 + (pch - digits);

			if (new > 255)
				return (0);
			*tp = new;
			if (! saw_digit)
			{
				if (++octets > 4)
					return (0);
				saw_digit = 1;
			}
		}
		else if (ch == '.' && saw_digit)
		{
			if (octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		}
		else
			return (0);
	}
	if (octets < 4)
		return (0);

	memcpy(dst, tmp, NS_INADDRSZ);
	return (1);
}

/* int
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 * credit:
 *	inspired by Mark Andrews.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton6(src, dst)
const char *src;
uint8_t *dst;
{
	static const char xdigits_l[] = "0123456789abcdef",
	                                xdigits_u[] = "0123456789ABCDEF";
	uint8_t tmp[NS_IN6ADDRSZ], *tp, *endp, *colonp;
	const char *xdigits, *curtok;
	int ch, saw_xdigit;
	uint32_t val;

	memset((tp = tmp), '\0', NS_IN6ADDRSZ);
	endp = tp + NS_IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while ((ch = *src++) != '\0')
	{
		const char *pch;

		if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL)
		{
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return (0);
			saw_xdigit = 1;
			continue;
		}
		if (ch == ':')
		{
			curtok = src;
			if (!saw_xdigit)
			{
				if (colonp)
					return (0);
				colonp = tp;
				continue;
			}
			if (tp + NS_INT16SZ > endp)
				return (0);
			*tp++ = (uint8_t) (val >> 8) & 0xff;
			*tp++ = (uint8_t) val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) &&
		        inet_pton4(curtok, tp) > 0)
		{
			tp += NS_INADDRSZ;
			saw_xdigit = 0;
			break;	/* '\0' was seen by inet_pton4(). */
		}
		return (0);
	}
	if (saw_xdigit)
	{
		if (tp + NS_INT16SZ > endp)
			return (0);
		*tp++ = (uint8_t) (val >> 8) & 0xff;
		*tp++ = (uint8_t) val & 0xff;
	}
	if (colonp != NULL)
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		for (i = 1; i <= n; i++)
		{
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return (0);
	memcpy(dst, tmp, NS_IN6ADDRSZ);
	return (1);
}


static const char *inet_ntop4(const u_char *src, char *dst, size_t size);
static const char *inet_ntop6(const u_char *src, char *dst, size_t size);

/* char *
 * inet_ntop(af, src, dst, size)
 *	convert a network format address to presentation format.
 * return:
 *	pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *	Paul Vixie, 1996.
 */
const char *
inet_ntop_PATCH(int af, const void *src, char *dst, size_t size)
{
	switch (af)
	{
	case AF_INET:
		return (inet_ntop4(src, dst, size));
	case AF_INET6:
		return (inet_ntop6(src, dst, size));
	default:
#ifdef EAFNOSUPPORT
		errno = EAFNOSUPPORT;
#else
		errno = ENOSYS;
#endif
		return (NULL);
	}
	/* NOTREACHED */
}

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address, more or less like inet_ntoa()
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a u_char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop4(const u_char *src, char *dst, size_t size)
{
	static const char fmt[] = "%u.%u.%u.%u";
	char tmp[sizeof "255.255.255.255"];
	int l;

	l = snprintf(tmp, size, fmt, src[0], src[1], src[2], src[3]);
	if (l <= 0 || l >= (int)size)
	{
		errno = ENOSPC;
		return (NULL);
	}
	strncpy(dst, tmp, size);
	return (dst);
}

/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop6(const u_char *src, char *dst, size_t size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
	char *tp, *ep;
	struct { int base, len; } best, cur;
	u_int words[IN6ADDRSZ / INT16SZ];
	int i;
	int advance;

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, '\0', sizeof words);
	for (i = 0; i < IN6ADDRSZ; i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	best.len = 0;
	cur.base = -1;
	cur.len = 0;
	for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++)
	{
		if (words[i] == 0)
		{
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		}
		else
		{
			if (cur.base != -1)
			{
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1)
	{
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	ep = tmp + sizeof(tmp);
	for (i = 0; i < (IN6ADDRSZ / INT16SZ) && tp < ep; i++)
	{
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base &&
		        i < (best.base + best.len))
		{
			if (i == best.base)
			{
				if (tp + 1 >= ep)
					return (NULL);
				*tp++ = ':';
			}
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0)
		{
			if (tp + 1 >= ep)
				return (NULL);
			*tp++ = ':';
		}
		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0 &&
		        (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
		{
			if (!inet_ntop4(src + 12, tp, (size_t)(ep - tp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		advance = snprintf(tp, ep - tp, "%x", words[i]);
		if (advance <= 0 || advance >= ep - tp)
			return (NULL);
		tp += advance;
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
	{
		if (tp + 1 >= ep)
			return (NULL);
		*tp++ = ':';
	}
	if (tp + 1 >= ep)
		return (NULL);
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */
	if ((size_t)(tp - tmp) > size)
	{
		errno = ENOSPC;
		return (NULL);
	}
	strncpy(dst, tmp, size);
	return (dst);
}

char *
strndup(const char *str, size_t n)
{
	size_t len;
	char *copy;

	for (len = 0; len < n && str[len]; len++)
		continue;
	if ((copy = malloc(len + 1)) == NULL)
		return NULL;
	(void)memcpy(copy, str, len);
	copy[len] = '\0';
	return copy;
}

#endif

