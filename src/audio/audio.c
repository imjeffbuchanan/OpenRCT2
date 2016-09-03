#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "../config.h"
#include "../interface/viewport.h"
#include "../interface/window.h"
#include "../localisation/language.h"
#include "../platform/platform.h"
#include "../ride/ride.h"
#include "../world/map.h"
#include "../world/sprite.h"
#include "audio.h"
#include "mixer.h"
#include "../openrct2.h"
#include "../util/util.h"
#include "../localisation/string_ids.h"

typedef struct rct_audio_params {
	bool in_range;
	int volume;
	int pan;
} rct_audio_params;

audio_device *gAudioDevices = NULL;
int gAudioDeviceCount;
int gAudioCurrentDevice = -1;
void *gCrowdSoundChannel = 0;
bool gGameSoundsOff = false;
void *gRainSoundChannel = 0;
rct_ride_music gRideMusicList[AUDIO_MAX_RIDE_MUSIC];
rct_ride_music_params gRideMusicParamsList[6];
rct_ride_music_params *gRideMusicParamsListEnd;
void *gTitleMusicChannel = 0;
rct_vehicle_sound gVehicleSoundList[AUDIO_MAX_VEHICLE_SOUNDS];
rct_vehicle_sound_params gVehicleSoundParamsList[AUDIO_MAX_VEHICLE_SOUNDS];
rct_vehicle_sound_params *gVehicleSoundParamsListEnd;
int gVolumeAdjustZoom = 0;

int _volumeAdjust[SOUND_MAXID] = {
	0,		// SOUND_LIFT_1
	0,		// SOUND_TRACK_FRICTION_1
	0,		// SOUND_LIFT_2
	0,		// SOUND_SCREAM_1
	0,		// SOUND_CLICK_1
	0,		// SOUND_CLICK_2
	0,		// SOUND_PLACE_ITEM
	0,		// SOUND_SCREAM_2
	0,		// SOUND_SCREAM_3
	0,		// SOUND_SCREAM_4
	0,		// SOUND_SCREAM_5
	0,		// SOUND_SCREAM_6
	0,		// SOUND_LIFT_3
	-400,	// SOUND_PURCHASE
	0,		// SOUND_CRASH
	0,		// SOUND_LAYING_OUT_WATER
	0,		// SOUND_WATER_1
	0,		// SOUND_WATER_2
	0,		// SOUND_TRAIN_WHISTLE
	0,		// SOUND_TRAIN_CHUGGING
	-1000,	// SOUND_WATER_SPLASH
	0,		// SOUND_HAMMERING
	-800,	// SOUND_RIDE_LAUNCH_1
	-1700,	// SOUND_RIDE_LAUNCH_2
	-700,	// SOUND_COUGH_1
	-700,	// SOUND_COUGH_2
	-700,	// SOUND_COUGH_3
	-700,	// SOUND_COUGH_4
	0,		// SOUND_RAIN_1
	0,		// SOUND_THUNDER_1
	0,		// SOUND_THUNDER_2
	0,		// SOUND_RAIN_2
	0,		// SOUND_RAIN_3
	0,		// SOUND_BALLOON_POP
	-700,	// SOUND_MECHANIC_FIX
	0,		// SOUND_SCREAM_7
	-1000,	// SOUND_TOILET_FLUSH
	0,		// SOUND_CLICK_3
	0,		// SOUND_QUACK
	0,		// SOUND_NEWS_ITEM
	0,		// SOUND_WINDOW_OPEN
	-900,	// SOUND_LAUGH_1
	-900,	// SOUND_LAUGH_2
	-900,	// SOUND_LAUGH_3
	0,		// SOUND_APPLAUSE
	-600,	// SOUND_HAUNTED_HOUSE_SCARE
	-700,	// SOUND_HAUNTED_HOUSE_SCREAM_1
	-700,	// SOUND_HAUNTED_HOUSE_SCREAM_2
	-2550,	// SOUND_48
	-2900,	// SOUND_49
	0,		// SOUND_ERROR
	-3400,	// SOUND_51
	0,		// SOUND_LIFT_4
	0,		// SOUND_LIFT_5
	0,		// SOUND_TRACK_FRICTION_2
	0,		// SOUND_LIFT_6
	0,		// SOUND_LIFT_7
	0,		// SOUND_TRACK_FRICTION_3
	0,		// SOUND_SCREAM_8
	0,		// SOUND_TRAM
	-2000,	// SOUND_DOOR_OPEN
	-2700,	// SOUND_DOOR_CLOSE
	-700	// SOUND_62
};

rct_audio_params audio_get_params_from_location(int soundId, const rct_xyz16 *location);
void audio_stop_channel(void **channel);

void audio_init()
{
	int result = SDL_Init(SDL_INIT_AUDIO);
	if (result >= 0)
		return;

	log_fatal("SDL_Init %s", SDL_GetError());
	exit(-1);
}

void audio_quit()
{
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_populate_devices()
{
	if (gAudioDevices != NULL)
		free(gAudioDevices);

	gAudioDeviceCount = SDL_GetNumAudioDevices(SDL_FALSE);
	if (gAudioDeviceCount <= 0)
		return;

	audio_device *systemAudioDevices = malloc(gAudioDeviceCount * sizeof(audio_device));
	for (int i = 0; i < gAudioDeviceCount; i++) {
		const char *utf8Name = SDL_GetAudioDeviceName(i, SDL_FALSE);
		if (utf8Name == NULL)
			utf8Name = language_get_string(STR_OPTIONS_SOUND_VALUE_UNKNOWN);

		safe_strcpy(systemAudioDevices[i].name, utf8Name, AUDIO_DEVICE_NAME_SIZE);
	}
#ifndef __LINUX__
	gAudioDeviceCount++;
	gAudioDevices = malloc(gAudioDeviceCount * sizeof(audio_device));
	safe_strcpy(gAudioDevices[0].name, language_get_string(STR_OPTIONS_SOUND_VALUE_DEFAULT), AUDIO_DEVICE_NAME_SIZE);
	memcpy(&gAudioDevices[1], systemAudioDevices, (gAudioDeviceCount - 1) * sizeof(audio_device));
#else
	gAudioDevices = malloc(gAudioDeviceCount * sizeof(audio_device));
	memcpy(gAudioDevices, systemAudioDevices, gAudioDeviceCount * sizeof(audio_device));
#endif // __LINUX__

	free(systemAudioDevices);
}

int audio_play_sound_panned(int soundId, int pan, sint16 x, sint16 y, sint16 z)
{
	if (pan == AUDIO_PLAY_AT_LOCATION)
		return audio_play_sound_at_location(soundId, x, y, z);

	return audio_play_sound(soundId, 0, pan);
}

int audio_play_sound_at_location(int soundId, sint16 x, sint16 y, sint16 z)
{
	if (gGameSoundsOff)
		return 0;

	rct_xyz16 location;
	location.x = x;
	location.y = y;
	location.z = z;

	rct_audio_params params = audio_get_params_from_location(soundId, &location);
	if (!params.in_range)
		return soundId;

	return audio_play_sound(soundId, params.volume, params.pan);
}

/**
* Returns the audio parameters to use when playing the specified sound at a virtual location.
* @param soundId The sound effect to be played.
* @param location The location at which the sound effect is to be played.
* @return The audio parameters to be used when playing this sound effect.
*/
rct_audio_params audio_get_params_from_location(int soundId, const rct_xyz16 *location)
{
	int volumeDown = 0;
	rct_audio_params params;
	params.in_range = true;
	params.volume = 0;
	params.pan = 0;

	rct_map_element *element = map_get_surface_element_at(location->x / 32, location->y / 32);
	if (element && (element->base_height * 8) - 5 > location->z)
		volumeDown = 10;

	uint8 rotation = get_current_rotation();
	rct_xy16 pos2 = coordinate_3d_to_2d(location, rotation);
	rct_window *window = gWindowNextSlot;
	while (true) {
		window--;
		if (window < g_window_list)
			break;

		rct_viewport *viewport = window->viewport;
		if (!viewport || !(viewport->flags & VIEWPORT_FLAG_SOUND_ON))
			continue;

		sint16 vy = pos2.y - viewport->view_y;
		sint16 vx = pos2.x - viewport->view_x;
		params.pan = viewport->x + (vx >> viewport->zoom);
		params.volume = _volumeAdjust[soundId] + ((-1024 * viewport->zoom - 1) << volumeDown) + 1;

		if (vy < 0 || vy >= viewport->view_height || vx < 0 || vx >= viewport->view_width || params.volume < -10000) {
			params.in_range = false;
			return params;
		}
	}

	return params;
}

int audio_play_sound(int soundId, int volume, int pan)
{
	if (gGameSoundsOff)
		return 0;

	int mixerPan = 0;
	if (pan != AUDIO_PLAY_AT_CENTRE) {
		int x2 = pan << 16;
		uint16 screenWidth = max(64, gScreenWidth);
		mixerPan = ((x2 / screenWidth) - 0x8000) >> 4;
			}

	Mixer_Play_Effect(soundId, MIXER_LOOP_NONE, DStoMixerVolume(volume), DStoMixerPan(mixerPan), 1, 1);
	return 0;
}

void audio_start_title_music()
{
	if (gGameSoundsOff || !(gScreenFlags & SCREEN_FLAGS_TITLE_DEMO)) {
		audio_stop_title_music();
		return;
	}

	if (gTitleMusicChannel)
		return;

	int pathId;
	switch (gConfigSound.title_music) {
	case 1:
		pathId = PATH_ID_CSS50;
		break;
	case 2:
		pathId = PATH_ID_CSS17;
		break;
	case 3:
		if (util_rand() & 1)
			pathId = PATH_ID_CSS50;
		else
			pathId = PATH_ID_CSS17;
		break;
	default:
		return;
	}

	gTitleMusicChannel = Mixer_Play_Music(pathId, MIXER_LOOP_INFINITE, true);
	if (gTitleMusicChannel != NULL) {
		Mixer_Channel_SetGroup(gTitleMusicChannel, MIXER_GROUP_TITLE_MUSIC);
	}
}

void audio_stop_ride_music()
{
	for (int i = 0; i < AUDIO_MAX_RIDE_MUSIC; i++) {
		rct_ride_music *rideMusic = &gRideMusicList[i];
		if (rideMusic->ride_id == (uint8)-1)
			continue;

		if (rideMusic->sound_channel)
			Mixer_Stop_Channel(rideMusic->sound_channel);

		rideMusic->ride_id = -1;
	}
}

void audio_stop_crowd_sound()
{
	audio_stop_channel(&gCrowdSoundChannel);
}

void audio_stop_title_music()
{
	audio_stop_channel(&gTitleMusicChannel);
}

void audio_stop_rain_sound()
{
	audio_stop_channel(&gRainSoundChannel);
}

/**
* Stops the specified audio channel from playing.
* @param channel The channel to stop.
*/
void audio_stop_channel(void **channel)
{
	if (!*channel)
		return;

	Mixer_Stop_Channel(*channel);
	*channel = 0;
}

void audio_init_ride_sounds_and_info()
{
	int deviceNum = 0;
	audio_init_ride_sounds(deviceNum);

	for (int m = 0; m < countof(gRideMusicInfoList); m++) {
		rct_ride_music_info *rideMusicInfo = gRideMusicInfoList[m];
		const utf8 *path = get_file_path(rideMusicInfo->path_id);
		SDL_RWops *file = SDL_RWFromFile(path, "rb");
		if (file == NULL)
			continue;

		uint32 head;
		SDL_RWread(file, &head, sizeof(head), 1);
		SDL_RWclose(file);
		if (head == 0x78787878)
			rideMusicInfo->length = 0;
	}
}

void audio_init_ride_sounds(int device)
{
	audio_close();
	for (int i = 0; i < AUDIO_MAX_VEHICLE_SOUNDS; i++) {
		rct_vehicle_sound *vehicleSound = &gVehicleSoundList[i];
		vehicleSound->id = -1;
	}

	gAudioCurrentDevice = device;
	config_save_default();
	for (int i = 0; i < AUDIO_MAX_RIDE_MUSIC; i++) {
		rct_ride_music *rideMusic = &gRideMusicList[i];
		rideMusic->ride_id = -1;
	}
}

void audio_close()
{
	audio_stop_crowd_sound();
	audio_stop_title_music();
	audio_stop_ride_music();
	audio_stop_rain_sound();
	gAudioCurrentDevice = -1;
}

void audio_toggle_all_sounds(){
	gConfigSound.sound_enabled = !gConfigSound.sound_enabled;
	if (gConfigSound.sound_enabled)
		audio_unpause_sounds();
	else {
		audio_stop_title_music();
		audio_pause_sounds();
	}
}

void audio_pause_sounds()
{
	gGameSoundsOff = true;
	audio_stop_vehicle_sounds();
	audio_stop_ride_music();
	audio_stop_crowd_sound();
	audio_stop_rain_sound();
}

void audio_unpause_sounds()
{
	gGameSoundsOff = false;
}

void audio_stop_vehicle_sounds()
{
	if (gOpenRCT2Headless || gAudioCurrentDevice == -1)
		return;

	for (int i = 0; i < countof(gVehicleSoundList); i++) {
		rct_vehicle_sound *vehicleSound = &gVehicleSoundList[i];
		if (vehicleSound->id == 0xFFFF)
			continue;

		if (vehicleSound->sound1_id != 0xFFFF)
			Mixer_Stop_Channel(vehicleSound->sound1_channel);

		if (vehicleSound->sound2_id != 0xFFFF)
			Mixer_Stop_Channel(vehicleSound->sound2_channel);

		vehicleSound->id = 0xFFFF;
	}
}
