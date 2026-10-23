/* HTTP and HTTPS on the Amiga (AmigaHttp.c): bsdsocket.library and AmiSSL 5.1 or newer. */
#ifndef ENCORE_AMIGA_HTTP_H
#define ENCORE_AMIGA_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sends `method` to `url` with `extraHeaders` ("Name: value\r\n" lines, or NULL) and `body`
   (or NULL), following redirects. 1 with the answer's body (malloc'd, the caller frees it) and
   its status; 0 with why in `error` when no answer came. */
int amigaHttpRequest(const char *method, const char *url, const char *extraHeaders, const void *body,
                     unsigned long bodySize, unsigned char **reply, unsigned long *replySize, long *status,
                     char *error, int errorSize);

#ifdef __cplusplus
}
#endif

#endif
