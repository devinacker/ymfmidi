#include <cstdio>

#define SDL_MAIN_HANDLED
extern "C" {
#include <SDL2/SDL.h>
}

#include "player.h"

static void audioCallback(void *data, uint8_t *stream, int len)
{
	memset(stream, 0, len);
	
	auto player = reinterpret_cast<OPLPlayer*>(data);
	player->generate(reinterpret_cast<float*>(stream), len / (2 * sizeof(float)));
}

int main(int argc, char **argv)
{
	setbuf(stdout, NULL);

	SDL_SetMainReady();
	
	auto player = new OPLPlayer;
	
	// init SDL audio now
	SDL_Init(SDL_INIT_AUDIO);
	
	SDL_AudioSpec want = {0};
	SDL_AudioSpec have = {0};
	
	want.freq = 44100;
	want.format = AUDIO_F32;
	want.channels = 2;
	want.samples = 4096;
	want.callback = audioCallback;
	want.userdata = player;
	SDL_OpenAudio(&want, &have);
	// TODO: make sure audio format is supported...
	
	// blah blah
	player->setSampleRate(have.freq);
	player->loadPatches("./GENMIDI.op2");
	
	SDL_PauseAudio(0);
	for (uint8_t patch = 0; patch < 128; patch++)
	{
		printf("midi patch %u: %s\n", patch, player->patchName(patch).c_str());
		player->midiProgramChange(0, patch);
	
		for (uint8_t note = 25; note < 65; note += 6)
		{
			printf("midi note %u\n", note);
			player->midiNoteOn(0, note, 127);
			SDL_Delay(400);
			player->midiNoteOff(0, note);
			SDL_Delay(100);
		}
	}
	SDL_Delay(2000);
	
	SDL_Quit();
	delete player;
	
	return 0;
}
