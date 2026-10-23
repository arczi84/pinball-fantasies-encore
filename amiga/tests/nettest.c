/* AmigaHttp.c by itself: GETs each address given and writes status, size or error to nettest.log. */
#include "platform/amiga/AmigaHttp.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    FILE *out = fopen("nettest.log", "w");
    int i;
    if (!out) return 1;
    for (i = 1; i < argc; ++i) {
        unsigned char *data = NULL;
        unsigned long size = 0;
        long status = 0;
        char error[256] = "";
        fprintf(out, "%s ...\n", argv[i]);
        fflush(out);
        if (amigaHttpRequest("GET", argv[i], NULL, NULL, 0, &data, &size, &status, error, sizeof error))
            fprintf(out, "  status %ld, %lu bytes: %.80s\n", status, size, data ? (char *)data : "");
        else
            fprintf(out, "  failed: %s\n", error);
        fflush(out);
        free(data);
    }
    fprintf(out, "done\n");
    fclose(out);
    return 0;
}
