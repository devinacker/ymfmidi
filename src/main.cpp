#include <cstdio>
#include <getopt.h>

#define SDL_MAIN_HANDLED
extern "C" {
#include <SDL2/SDL.h>
}

#include "player.h"

static bool g_running = true;

// ----------------------------------------------------------------------------
static void cls()
{
	printf("\x1b[2J");
}

// ----------------------------------------------------------------------------
static void audioCallback(void *data, uint8_t *stream, int len)
{
	memset(stream, 0, len);
	
	auto player = reinterpret_cast<OPLPlayer*>(data);
	player->generate(reinterpret_cast<float*>(stream), len / (2 * sizeof(float)));
}

// ----------------------------------------------------------------------------
void usage()
{
	fprintf(stderr, 
	"usage: ymfm-test [options] song_path [patch_path]\n"
	"\n"
	"supported song formats:  MID, MUS\n"
	"supported patch formats: WOPL, OP2\n"
	"\n"
	"supported options:\n"
	"  -h / --help             show this information and exit\n"
	"  -b / --buf <num>        set buffer size (default 4096)\n"
	"  -g / --gain <num>       set gain amount (default 1.0)\n"
	"  -r / --rate <num>       set sample rate (default 44100)\n"
	"\n"
	);
	
	exit(1);
}

static const option options[] = 
{
	{"help", 0, nullptr, 'h'},
	{"buf",  1, nullptr, 'b'},
	{"gain", 1, nullptr, 'g'},
	{"rate", 1, nullptr, 'r'},
	{0}
};

// ----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	
	const char* songPath;
	const char* patchPath = "./GENMIDI.wopl";
	int sampleRate = 44100;
	int bufferSize = 4096;
	double gain = 1.0;

	char opt;
	while ((opt = getopt_long(argc, argv, ":hb:g:r:", options, nullptr)) != -1)
	{
		switch (opt)
		{
		case ':':
		case 'h':
			usage();
			break;
		
		case 'b':
			bufferSize = atoi(optarg);
			printf("bufferSize = %s\n", optarg);
			if (!bufferSize)
			{
				fprintf(stderr, "invalid buffer size: %s\n", optarg);
				exit(1);
			}
			break;
		
		case 'g':
			gain = atof(optarg);
			printf("gain = %s\n", optarg);
			if (!gain)
			{
				fprintf(stderr, "invalid gain: %s\n", optarg);
				exit(1);
			}
			break;
		
		case 'r':
			sampleRate = atoi(optarg);
			printf("sampleRate = %s\n", optarg);
			if (!sampleRate)
			{
				fprintf(stderr, "invalid sample rate: %s\n", optarg);
				exit(1);
			}
			break;
		}
	}
	
	if (optind >= argc)
		usage();
	
	songPath = argv[optind];
	if (optind + 1 < argc)
		patchPath = argv[optind + 1];
	
	auto player = new OPLPlayer;
	
	if (!player->loadSequence(songPath))
	{
		fprintf(stderr, "couldn't load %s\n", songPath);
		exit(1);
	}
	
	if (!player->loadPatches(patchPath))
	{
		fprintf(stderr, "couldn't load %s\n", patchPath);
		exit(1);
	}
	
	// init SDL audio now
	SDL_SetMainReady();
	SDL_Init(SDL_INIT_AUDIO);
	
	SDL_AudioSpec want = {0};
	SDL_AudioSpec have = {0};
	
	want.freq = sampleRate;
	want.format = AUDIO_F32;
	want.channels = 2;
	want.samples = bufferSize;
	want.callback = audioCallback;
	want.userdata = player;
	SDL_OpenAudio(&want, &have);
	// TODO: make sure audio format is supported...
	
	// blah blah
	player->setSampleRate(have.freq);
	player->setGain(gain);
	SDL_PauseAudio(0);
	
	cls();

	while (g_running)
	{
		player->display();
		SDL_Delay(50);
	}

	cls();
	SDL_Quit();
	delete player;
	
	return 0;
}
