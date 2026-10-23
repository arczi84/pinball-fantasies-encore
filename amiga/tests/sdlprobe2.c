/* As the game starts SDL: metadata, then one SDL_Init of all three; optionally with libnix's
   stack swap (STACKSWAP: __stack and -Wl,-u,___StackSwap), as the game links. */
#include <SDL3/SDL.h>
#include <stdio.h>
#ifdef STACKSWAP
long __stack = 1024 * 1024;
#endif
int main(int argc, char **argv) {
    FILE *out = fopen(argc > 1 ? argv[1] : "sdlprobe2.log", "w");
    if (!out) return 1;
    fprintf(out, "metadata\n"); fflush(out);
    SDL_SetAppMetadata("Pinball Fantasies: Encore!", NULL, "org.encore.pinball-fantasies");
    fprintf(out, "init...\n"); fflush(out);
    fprintf(out, "init: %s\n", SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) ? "ok" : SDL_GetError()); fflush(out);
    SDL_Quit();
    fprintf(out, "done\n");
    fclose(out);
    return 0;
}
