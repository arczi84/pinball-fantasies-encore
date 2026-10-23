/* HTTP and HTTPS for the Amiga port (platform/Http.cpp): bsdsocket.library (any TCP/IP stack,
   Roadshow, WinUAE's bsdsocket emulation) and AmiSSL 5.1 or newer, opened at the first request
   and closed when the program ends. In C, so that the stack's and AmiSSL's inline macros
   (connect, send, SSL_read...) stay away from the C++ code.

   A request is sent with "Connection: close" and read to the end; the answer may come in chunks
   (Transfer-Encoding: chunked), and a redirect (301, 302, 303, 307, 308) is followed, up to
   five times. The server's certificate is checked against AmiSSL's certificates
   (AMISSL:Certs) and its name against the host's. */
#include "platform/amiga/AmigaHttp.h"

#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *SocketBase;
struct Library *AmiSSLMasterBase, *AmiSSLBase, *AmiSSLExtBase;

/* Each: 0 not tried, 1 open, -1 failed (its g_why... says why). The TCP/IP stack is opened for
   any request, AmiSSL only for the first https one. */
static int g_socketState, g_sslState;
static char g_socketWhy[160], g_sslWhy[160];
static SSL_CTX *g_ctx;

static void fail(char *error, int size, const char *format, ...) {
    va_list args;
    if (!error || size <= 0) return;
    va_start(args, format);
    vsnprintf(error, (size_t)size, format, args);
    va_end(args);
}

static void close_ssl(void) {
    if (g_ctx) SSL_CTX_free(g_ctx);
    g_ctx = NULL;
    if (AmiSSLBase) CloseAmiSSL();
    AmiSSLBase = AmiSSLExtBase = NULL;
    if (AmiSSLMasterBase) CloseLibrary(AmiSSLMasterBase);
    AmiSSLMasterBase = NULL;
}

static void close_network(void) {
    close_ssl();
    if (SocketBase) CloseLibrary(SocketBase);
    SocketBase = NULL;
}

static int open_sockets(void) {
    if (g_socketState) return g_socketState > 0;
    g_socketState = -1;
    if (!(SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4))) {
        snprintf(g_socketWhy, sizeof g_socketWhy, "no TCP/IP stack (bsdsocket.library)");
        return 0;
    }
    atexit(close_network);
    g_socketState = 1;
    return 1;
}

static int open_ssl(void) {
    if (g_sslState) return g_sslState > 0;
    g_sslState = -1;
    if (!(AmiSSLMasterBase = OpenLibrary((CONST_STRPTR)"amisslmaster.library", AMISSLMASTER_MIN_VERSION))) {
        snprintf(g_sslWhy, sizeof g_sslWhy, "AmiSSL 5 is not installed (amisslmaster.library)");
        return 0;
    }
    if (OpenAmiSSLTags(AMISSL_V3xx,
                       AmiSSL_UsesOpenSSLStructs, FALSE,
                       AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
                       AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
                       AmiSSL_SocketBase, (ULONG)SocketBase,
                       AmiSSL_ErrNoPtr, (ULONG)__errno,
                       TAG_DONE) != 0) {
        snprintf(g_sslWhy, sizeof g_sslWhy, "AmiSSL 5.1 or newer could not be opened");
        close_ssl();
        return 0;
    }
    if (!(g_ctx = SSL_CTX_new(TLS_client_method()))) {
        snprintf(g_sslWhy, sizeof g_sslWhy, "AmiSSL: no TLS context");
        close_ssl();
        return 0;
    }
    SSL_CTX_set_default_verify_paths(g_ctx);
    SSL_CTX_set_verify(g_ctx, SSL_VERIFY_PEER, NULL);
    g_sslState = 1;
    return 1;
}

/* ------------------------------------------------------------------------------------------ */

typedef struct {
    int https;
    char host[256];
    int port;
    char path[2048];
} Url;

static int parse_url(const char *url, Url *u) {
    const char *p, *host_end, *slash;
    size_t n;
    memset(u, 0, sizeof *u);
    if (!strncmp(url, "https://", 8)) {
        u->https = 1, u->port = 443, p = url + 8;
    } else if (!strncmp(url, "http://", 7)) {
        u->port = 80, p = url + 7;
    } else {
        return 0;
    }
    slash = strchr(p, '/');
    host_end = slash ? slash : p + strlen(p);
    n = (size_t)(host_end - p);
    {
        const char *colon = memchr(p, ':', n);
        if (colon) {
            u->port = atoi(colon + 1);
            n = (size_t)(colon - p);
        }
    }
    if (n == 0 || n >= sizeof u->host) return 0;
    memcpy(u->host, p, n);
    u->host[n] = 0;
    snprintf(u->path, sizeof u->path, "%s", slash ? slash : "/");
    return 1;
}

/* One connection: a socket, and over it TLS for https. */
typedef struct {
    long sock;
    SSL *ssl;
} Conn;

static void conn_close(Conn *c) {
    if (c->ssl) {
        SSL_shutdown(c->ssl);
        SSL_free(c->ssl);
    }
    if (c->sock >= 0) CloseSocket(c->sock);
    c->ssl = NULL;
    c->sock = -1;
}

static int conn_open(Conn *c, const Url *u, char *error, int size) {
    struct hostent *he;
    struct sockaddr_in addr;
    c->sock = -1;
    c->ssl = NULL;
    if (!(he = gethostbyname((STRPTR)u->host))) {
        fail(error, size, "cannot find %s", u->host);
        return 0;
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)u->port);
    memcpy(&addr.sin_addr, he->h_addr, (size_t)he->h_length);
    if ((c->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fail(error, size, "no socket");
        return 0;
    }
    if (connect(c->sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        fail(error, size, "cannot connect to %s:%d", u->host, u->port);
        conn_close(c);
        return 0;
    }
    if (!u->https) return 1;
    if (!open_ssl()) {
        fail(error, size, "%s", g_sslWhy);
        conn_close(c);
        return 0;
    }
    if (!(c->ssl = SSL_new(g_ctx))) {
        fail(error, size, "AmiSSL: no TLS session");
        conn_close(c);
        return 0;
    }
    SSL_set_fd(c->ssl, (int)c->sock);
    SSL_set_tlsext_host_name(c->ssl, u->host);
    SSL_set1_host(c->ssl, u->host);
    if (SSL_connect(c->ssl) != 1) {
        long verify = SSL_get_verify_result(c->ssl);
        if (verify != X509_V_OK)
            fail(error, size, "the certificate of %s is not trusted (%s)", u->host,
                 X509_verify_cert_error_string(verify));
        else
            fail(error, size, "TLS with %s failed", u->host);
        conn_close(c);
        return 0;
    }
    return 1;
}

static int conn_write(Conn *c, const void *data, unsigned long size) {
    const char *p = (const char *)data;
    while (size > 0) {
        int chunk = size > 16384 ? 16384 : (int)size;
        int sent = c->ssl ? SSL_write(c->ssl, p, chunk) : (int)send(c->sock, (APTR)p, chunk, 0);
        if (sent <= 0) return 0;
        p += sent;
        size -= (unsigned long)sent;
    }
    return 1;
}

static int conn_read(Conn *c, void *data, int size) {
    return c->ssl ? SSL_read(c->ssl, data, size) : (int)recv(c->sock, data, size, 0);
}

/* A growing buffer. */
typedef struct {
    unsigned char *data;
    unsigned long size, room;
} Buffer;

static int buffer_add(Buffer *b, const void *data, unsigned long size) {
    if (b->size + size + 1 > b->room) {
        unsigned long room = b->room ? b->room : 65536;
        unsigned char *bigger;
        while (b->size + size + 1 > room) room *= 2;
        if (!(bigger = realloc(b->data, room))) return 0;
        b->data = bigger;
        b->room = room;
    }
    memcpy(b->data + b->size, data, size);
    b->size += size;
    b->data[b->size] = 0;
    return 1;
}

/* The value of a header ("Name:"), or NULL; the headers end at `end`. */
static const char *header(const char *headers, const char *end, const char *name, char *value, size_t size) {
    size_t n = strlen(name);
    const char *line = headers;
    while (line < end) {
        const char *eol = strstr(line, "\r\n");
        if (!eol || eol > end) eol = end;
        if ((size_t)(eol - line) > n && !strncasecmp(line, name, n) && line[n] == ':') {
            const char *v = line + n + 1;
            size_t len;
            while (v < eol && (*v == ' ' || *v == '\t')) ++v;
            len = (size_t)(eol - v);
            if (len >= size) len = size - 1;
            memcpy(value, v, len);
            value[len] = 0;
            return value;
        }
        line = eol + 2;
    }
    return NULL;
}

/* Undoes Transfer-Encoding: chunked, in place. */
static int dechunk(unsigned char *data, unsigned long *size) {
    unsigned long in = 0, out = 0;
    for (;;) {
        unsigned long len;
        char *end;
        if (in >= *size) return 0;
        len = strtoul((const char *)data + in, &end, 16);
        while ((unsigned char *)end < data + *size && *end != '\n') ++end;
        in = (unsigned long)((unsigned char *)end - data) + 1;
        if (len == 0) break;
        if (in + len > *size) return 0;
        memmove(data + out, data + in, len);
        out += len;
        in += len + 2;  /* the chunk's CRLF */
    }
    *size = out;
    return 1;
}

int amigaHttpRequest(const char *method, const char *url, const char *extraHeaders, const void *body,
                     unsigned long bodySize, unsigned char **reply, unsigned long *replySize, long *status,
                     char *error, int errorSize) {
    char where[2048];
    int redirects;
    *reply = NULL;
    *replySize = 0;
    *status = 0;
    if (!open_sockets()) {
        fail(error, errorSize, "%s", g_socketWhy);
        return 0;
    }
    snprintf(where, sizeof where, "%s", url);
    for (redirects = 0; redirects <= 5; ++redirects) {
        Url u;
        Conn c;
        Buffer request = {0}, answer = {0};
        char line[2560], value[2048];
        const char *head_end;
        unsigned long head_size;
        char buf[16384];
        int got;

        if (!parse_url(where, &u)) {
            fail(error, errorSize, "not a web address: %s", where);
            return 0;
        }
        if (!conn_open(&c, &u, error, errorSize)) return 0;
        snprintf(line, sizeof line, "%s %s HTTP/1.1\r\nHost: %s\r\n", method, u.path, u.host);
        buffer_add(&request, line, strlen(line));
        snprintf(line, sizeof line,
                 "User-Agent: PinballFantasiesEncore-Amiga\r\nAccept-Encoding: identity\r\nConnection: close\r\n");
        buffer_add(&request, line, strlen(line));
        if (extraHeaders) buffer_add(&request, extraHeaders, strlen(extraHeaders));
        if (body) {
            snprintf(line, sizeof line, "Content-Length: %lu\r\n", bodySize);
            buffer_add(&request, line, strlen(line));
        }
        if (!buffer_add(&request, "\r\n", 2) || (body && !buffer_add(&request, body, bodySize))) {
            fail(error, errorSize, "out of memory");
            conn_close(&c);
            free(request.data);
            return 0;
        }
        if (!conn_write(&c, request.data, request.size)) {
            fail(error, errorSize, "the request to %s could not be sent", u.host);
            conn_close(&c);
            free(request.data);
            return 0;
        }
        free(request.data);
        while ((got = conn_read(&c, buf, (int)sizeof buf)) > 0) {
            if (!buffer_add(&answer, buf, (unsigned long)got)) {
                fail(error, errorSize, "out of memory");
                conn_close(&c);
                free(answer.data);
                return 0;
            }
        }
        conn_close(&c);
        if (!answer.data || !(head_end = strstr((const char *)answer.data, "\r\n\r\n"))) {
            fail(error, errorSize, "no answer from %s", u.host);
            free(answer.data);
            return 0;
        }
        head_size = (unsigned long)(head_end - (const char *)answer.data) + 4;
        if (sscanf((const char *)answer.data, "HTTP/%*s %ld", status) != 1) {
            fail(error, errorSize, "%s did not answer in HTTP", u.host);
            free(answer.data);
            return 0;
        }
        if ((*status == 301 || *status == 302 || *status == 303 || *status == 307 || *status == 308) &&
            header((const char *)answer.data, head_end, "Location", value, sizeof value)) {
            free(answer.data);
            if (value[0] == '/')  /* (a path on the same server) */
                snprintf(where, sizeof where, "%s://%s:%d%s", u.https ? "https" : "http", u.host, u.port, value);
            else
                snprintf(where, sizeof where, "%s", value);
            if (*status == 303) method = "GET", body = NULL;
            continue;
        }
        {
            /* The body, moved to the start of the buffer, and put together if it came in chunks. */
            const int chunked = header((const char *)answer.data, head_end, "Transfer-Encoding", value, sizeof value) &&
                                strstr(value, "chunked") != NULL;
            memmove(answer.data, answer.data + head_size, answer.size - head_size);
            answer.size -= head_size;
            if (chunked && !dechunk(answer.data, &answer.size)) {
                fail(error, errorSize, "the answer from %s was cut short", u.host);
                free(answer.data);
                return 0;
            }
        }
        *reply = answer.data;
        *replySize = answer.size;
        return 1;
    }
    fail(error, errorSize, "too many redirects");
    return 0;
}
