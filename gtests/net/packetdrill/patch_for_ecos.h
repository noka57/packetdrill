#ifdef ECOS

#ifndef POLL_VAR
#define POLL_VAR
typedef unsigned int	nfds_t;
typedef struct pollfd
{
	int 	fd;
	short	events;
	short	revents;
} pollfd_t;
#endif

int inet_pton_PATCH (int, const char *, void *);
const char *inet_ntop_PATCH (int, const void *, char *, size_t);
char * strndup(const char *, size_t);
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

//FILE *open_memstream(char **bufp, size_t *sizep);
char *get_available_file_name(void);

ssize_t sendfile_PATCH(int out_fd, int in_fd, off_t *offset, size_t count);

#define TUN_PATH                "/dev/tun0"
#define TUN_DEV                 "tun0"

#define	POLLIN		0x0001  	/* any readable data available */
#define	POLLPRI		0x0002		/* OOB/Urgent readable data */
#define	POLLOUT		0x0004 		/* file descriptor is writeable */
#define	POLLERR		0x0008  	/* some poll error occurred */
#define	POLLHUP		0x0010 		/* file descriptor was "hung up" */
#define	POLLNVAL	0x0020 		/* requested events "invalid" */

#endif
