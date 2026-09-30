/* sdlbeep - SDL3's DOS audio (the Sound Blaster driver: DMA, IRQ, and the
 * ring its cooperative audio thread fills): a 440 Hz tone for two seconds.
 * Loop A runs it with an SB16, BLASTER set and --wav, and looks for the tone
 * in the recording (tools/sdl/loopa.sh). SDL_Delay is what lets the audio
 * thread run: the scheduler switches only at yields. */
#include "hx.h"
#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define RATE 22050

int main(int argc, char **argv)
{
    static Sint16 tone[RATE * 2];
    SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, RATE };
    SDL_AudioStream *s;
    Uint64 end;
    int i;

    hx_init(argc, argv, "sdlbeep");
    hx_test("init", SDL_Init(SDL_INIT_AUDIO), "%s", SDL_GetError());
    if (hx_failures())
        hx_done(1);
    hx_log("audio driver %s", SDL_GetCurrentAudioDriver());
    s = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    hx_test("open", s != NULL, "%s", SDL_GetError());
    if (!s)
        hx_done(1);
    for (i = 0; i < RATE * 2; i++)
        tone[i] = (Sint16)(12000.0 * sin(2.0 * M_PI * 440.0 * i / RATE));
    hx_test("queue", SDL_PutAudioStreamData(s, tone, sizeof tone), "%s", SDL_GetError());
    SDL_FlushAudioStream(s);                    /* no more data: let the resampler finish the tail */
    SDL_ResumeAudioStreamDevice(s);
    end = SDL_GetTicks() + 6000;
    while (SDL_GetAudioStreamQueued(s) > 0 && SDL_GetTicks() < end)
        SDL_Delay(10);
    hx_test("drained", SDL_GetAudioStreamQueued(s) == 0, "%d bytes left", SDL_GetAudioStreamQueued(s));
    SDL_Delay(300);                             /* let the ring and the DMA buffer play out */
    SDL_DestroyAudioStream(s);
    SDL_Quit();
    hx_done(hx_failures() ? 1 : 0);
    return 0;
}
