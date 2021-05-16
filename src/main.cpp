#include <cstdio>

#define SDL_MAIN_HANDLED
extern "C" {
#include <SDL2/SDL.h>
}

#include "player.h"

static void cls()
{
	printf("\x1b[2J");
}

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
	
	if (argc < 2)
	{
		printf("usage: %s path_to_song [path_to_patches]\n", argv[0]);
		printf("supported song formats:  MUS\n");
		printf("supported patch formats: OP2\n");
		
		exit(0);
	}
	
	if (!player->loadSequence(argv[1]))
	{
		printf("couldn't load %s\n", argv[1]);
		exit(1);
	}
	
	const char* patchPath = "./GENMIDI.op2";
	if (argc >= 3)
		patchPath = argv[2];
	
	if (!player->loadPatches(patchPath))
	{
		printf("couldn't load %s\n", patchPath);
		exit(1);
	}
	
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
	SDL_PauseAudio(0);
	
	cls();
	
	while(1)
	{
		player->display();
		SDL_Delay(50);
	}

	cls();
	SDL_Quit();
	delete player;
	
	return 0;
}
