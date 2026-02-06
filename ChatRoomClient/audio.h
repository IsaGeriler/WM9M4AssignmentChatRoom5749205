#pragma once

#include <iostream>
#include <string>

#include <fmod.hpp>
#include <fmod_errors.h>

#pragma comment(lib, "fmod_vc.lib")

void playSound(FMOD::System* system, std::string sound_path) {
	// Declare the sound and channel pointers
	FMOD::Sound* sound = NULL;
	FMOD::Channel* channel = NULL;

	// Create and play the sound (only once, per received message)
	system->createSound(sound_path.c_str(), FMOD_DEFAULT, NULL, &sound);
	system->playSound(sound, NULL, false, &channel);
	
	// Release the pointer to deallocate memory
	sound->release();
}