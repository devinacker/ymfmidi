#include <cstdio>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#define SDL_MAIN_HANDLED
extern "C" {
#include <SDL2/SDL.h>
}

#include "console.h"
#include "player.h"

static bool g_running = true;
static bool g_paused = false;
static bool g_looping = true;

// ----------------------------------------------------------------------------
static void audioCallback(void *data, uint8_t *stream, int len)
{
	memset(stream, 0, len);
	
	auto player = reinterpret_cast<OPLPlayer*>(data);
	player->generate(reinterpret_cast<float*>(stream), len / (2 * sizeof(float)));
	
	if (!g_looping)
		g_running &= !player->atEnd();
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
	"  -q / --quiet            quiet (run non-interactively)\n"
	"  -1 / --play-once        play only once and then exit\n"
	"\n"
	"  -n / --num <num>        set number of chips (default 1)\n"
	"  -b / --buf <num>        set buffer size (default 4096)\n"
	"  -g / --gain <num>       set gain amount (default 1.0)\n"
	"  -r / --rate <num>       set sample rate (default 44100)\n"
	"\n"
	);
	
	exit(1);
}

static const option options[] = 
{
	{"help",      0, nullptr, 'h'},
	{"quiet",     0, nullptr, 'q'},
	{"play-once", 0, nullptr, '1'}, 
	{"num",       1, nullptr, 'n'},
	{"buf",       1, nullptr, 'b'},
	{"gain",      1, nullptr, 'g'},
	{"rate",      1, nullptr, 'r'},
	{0}
};

// ----------------------------------------------------------------------------
void quit(int)
{
	g_running = false;
}

// ----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	
	bool interactive = true;
	unsigned displayType = 0;
	
	const char* songPath;
	const char* patchPath = "GENMIDI.wopl";
	int sampleRate = 44100;
	int bufferSize = 4096;
	double gain = 1.0;
	int numChips = 1;

	char opt;
	while ((opt = getopt_long(argc, argv, ":hq1n:b:g:r:", options, nullptr)) != -1)
	{
		switch (opt)
		{
		case ':':
		case 'h':
			usage();
			break;
		
		case 'q':
			interactive = false;
			break;
		
		case '1':
			g_looping = false;
			break;
		
		case 'n':
			numChips = atoi(optarg);
			if (numChips < 1)
			{
				fprintf(stderr, "number of chips must be at least 1\n");
				exit(1);
			}
		
		case 'b':
			bufferSize = atoi(optarg);
			if (!bufferSize)
			{
				fprintf(stderr, "invalid buffer size: %s\n", optarg);
				exit(1);
			}
			break;
		
		case 'g':
			gain = atof(optarg);
			if (!gain)
			{
				fprintf(stderr, "invalid gain: %s\n", optarg);
				exit(1);
			}
			break;
		
		case 'r':
			sampleRate = atoi(optarg);
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
	
	auto player = new OPLPlayer(numChips);
	
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
	player->setLoop(g_looping);
	player->setSampleRate(have.freq);
	player->setGain(gain);
	SDL_PauseAudio(0);
	
	if (interactive)
	{
		consoleOpen();
		consolePos(0);
	}
	
	printf("song:    %s\n", songPath);
	printf("patches: %s\n", patchPath);

	signal(SIGINT, quit);

	if (interactive)
	{
		printf("\ncontrols: [p] pause, [r] restart, [tab] change view, [esc/q] quit\n");
	}

	while (g_running)
	{
		if (interactive)
		{
			consolePos(5);
			
			if (!displayType)
				player->displayChannels();
			else
				player->displayVoices();
			
			switch (consoleGetKey())
			{
			case 0x1b:
			case 'q':
				g_running = false;
				continue;
			
			case 'p':
				g_paused ^= true;
				SDL_PauseAudio(g_paused);
				break;
			
			case 'r':
				g_paused = false;
				SDL_PauseAudio(0);
				player->reset();
				break;
				
			case 0x09:
				displayType ^= 1;
				consolePos(5);
				player->displayClear();
				break;
			}
		}
		SDL_Delay(30);
	}

	SDL_Quit();
	delete player;
	
	return 0;
}
