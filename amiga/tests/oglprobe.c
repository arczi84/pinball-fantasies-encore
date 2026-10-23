/* Why OGLES2Open fails: each way of opening the library in turn, into oglprobe.log. */
#include <proto/exec.h>
#include <proto/ogles2.h>
#include <stdio.h>

static void try_open(FILE *out, const char *name) {
    struct Library *lib = OpenLibrary((CONST_STRPTR)name, 0);
    fprintf(out, "%s: %p", name, (void *)lib);
    if (lib) {
        fprintf(out, " v%d.%d", lib->lib_Version, lib->lib_Revision);
        CloseLibrary(lib);
    }
    fprintf(out, "\n");
    fflush(out);
}

int main(void) {
    FILE *out = fopen("oglprobe.log", "w");
    if (!out) return 1;
    try_open(out, "uaenative.library");
    try_open(out, "ogles2.library");
    try_open(out, "PROGDIR:libs/ogles2.library");
    fprintf(out, "OGLES2Open: %d base %p\n", (int)OGLES2Open(), (void *)OGLES2Base);
    fflush(out);
    OGLES2Close();
    fclose(out);
    return 0;
}
