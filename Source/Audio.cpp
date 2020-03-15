#include "Audio.h"
#include "Defs.h"
#include "Log.h"
#include "Application.h"

#include "optick-1.3.0.0/include/optick.h"
#include "SDL/include/SDL.h"

#include "SDL2_mixer-2.0.4/include/SDL_mixer.h"
#ifdef PLATFORMx86
#pragma comment( lib, "SDL2_mixer-2.0.4/lib/x86/SDL2_mixer.lib" )
#elif PLATFORMx64
#pragma comment( lib, "SDL2_mixer-2.0.4/lib/x64/SDL2_mixer.lib" )
#endif


Audio::Audio() : Module("audio")
{}

// Destructor
Audio::~Audio()
{}

// Called before render is available
bool Audio::Awake(pugi::xml_node& config)
{
	OPTICK_EVENT();

	LOG("Loading Audio Mixer");
	bool ret = true;
	SDL_Init(0);

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		LOG("SDL_INIT_AUDIO could not initialize! SDL_Error: %s\n", SDL_GetError());
		active = false;
		ret = true;
	}

	// load support for the JPG and PNG image formats
	int flags = MIX_INIT_OGG;
	int init = Mix_Init(flags);

	if((init & flags) != flags)
	{
		LOG("Could not initialize Mixer lib. Mix_Init: %s", Mix_GetError());
		active = false;
		ret = true;
	}

	//Initialize SDL_mixer
	if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
	{
		LOG("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
		active = false;
		ret = true;
	}

	return ret;
}

// Called before quitting
bool Audio::CleanUp()
{
	if(!active)
		return true;

	LOG("Freeing sound FX, closing Mixer and Audio subsystem");

	if(music) Mix_FreeMusic(music);

	for(std::vector<Mix_Chunk*>::iterator it = fx.begin(); it != fx.end(); ++it)
		Mix_FreeChunk(*it);

	fx.clear();

	Mix_CloseAudio();
	Mix_Quit();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	return true;
}

// Play a music file
bool Audio::PlayMusic(const char* path, float fade_time)
{
	OPTICK_EVENT();

	bool ret = true;

	if(!active)
		return false;

	if(music)
	{
		if(fade_time > 0.0f)
			Mix_FadeOutMusic(int(fade_time * 1000.0f));
		else
			Mix_HaltMusic();

		// this call blocks until fade out is done
		Mix_FreeMusic(music);
	}

	music = Mix_LoadMUS_RW(App->files.Load(path), 1);

	if(music)
	{
		if (fade_time > 0.0f)
		{
			if (Mix_FadeInMusic(music, -1, (int)(fade_time * 1000.0f)) < 0)
			{
				LOG("Cannot fade in music %s. Mix_GetError(): %s", path, Mix_GetError());
				ret = false;
			}
		}
		else
		{
			if (Mix_PlayMusic(music, -1) < 0)
			{
				LOG("Cannot play in music %s. Mix_GetError(): %s", path, Mix_GetError());
				ret = false;
			}
		}
	}
	else
	{
		LOG("Cannot load music %s. Mix_GetError(): %s\n", path, Mix_GetError());
		ret = false;
	}

	LOG("Successfully playing %s", path);
	return ret;
}

// Load WAV
unsigned int Audio::LoadFx(const char* path)
{
	OPTICK_EVENT();

	unsigned int ret = 0;

	if(!active)
		return 0;

	std::string audio_path = App->files.GetBasePath();
	audio_path += path;
	Mix_Chunk* chunk = Mix_LoadWAV(audio_path.c_str());

	if(chunk)
	{
		fx.push_back(chunk);
		ret = fx.size();
	}
	else
	{
		LOG("Cannot load wav %s. Mix_GetError(): %s", audio_path.c_str(), Mix_GetError());
	}

	return ret;
}

// Play WAV
bool Audio::PlayFx(unsigned int id, int repeat)
{
	OPTICK_EVENT();

	bool ret = (active && id > 0 && id <= fx.size());

	if (ret) Mix_PlayChannel(-1, fx[id - 1], repeat);

	return ret;
}