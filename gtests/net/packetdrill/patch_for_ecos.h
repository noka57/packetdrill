#ifdef ECOS

int inet_pton (int, const char *, void *);
const char *inet_ntop (int, const void *, char *, size_t);
char * strndup(const char *, size_t);

#define TUN_PATH                "/dev/tun0"
#define TUN_DEV                 "tun0"

#endif
