#ifdef ECOS

int inet_pton (int, const char *, void *);
const char *inet_ntop (int, const void *, char *, size_t);
char * strndup(const char *, size_t);

//FILE *open_memstream(char **bufp, size_t *sizep);
char *get_available_file_name(void);

#define TUN_PATH                "/dev/tun0"
#define TUN_DEV                 "tun0"

#endif
