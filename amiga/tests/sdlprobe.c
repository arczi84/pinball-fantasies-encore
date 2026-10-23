/* Which SDL3 subsystem fails to start: each in turn, written to sdlprobe.log as it goes. */
#include <SDL3/SDL.h>
#include <stdio.h>

static FILE *out;
static void step(const char *what, SDL_InitFlags flags) {
    fprintf(out, "%s...\n", what);
    fflush(out);
    fprintf(out, "%s: %s\n", what, SDL_InitSubSystem(flags) ? "ok" : SDL_GetError());
    fflush(out);
}

int main(int argc, char **argv) {
    out = fopen("sdlprobe.log", "w");
    if (!out) return 1;
    step("video", SDL_INIT_VIDEO);
    step("audio", SDL_INIT_AUDIO);
    step("gamepad", SDL_INIT_GAMEPAD);
    SDL_Quit();
    fprintf(out, "done\n");
    fclose(out);
    return 0;
}
