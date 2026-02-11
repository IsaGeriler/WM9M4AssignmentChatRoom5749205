#pragma once

#include <iostream>

#include <FMOD/fmod.hpp>
#include <FMOD/fmod_errors.h>

#pragma comment(lib, "fmod_vc.lib")

class Sound {
private:
	FMOD::Channel* broadcastChannel = NULL;
	FMOD::Channel* dmChannel = NULL;
	FMOD::Channel* serverChannel = NULL;

	FMOD::Sound* broadcastSound = NULL;
	FMOD::Sound* dmSound = NULL;
	FMOD::Sound* serverSound = NULL;

	FMOD::System* system = NULL;
public:
	Sound() {
		FMOD::System_Create(&system);
		system->init(512, FMOD_INIT_NORMAL, NULL);
		system->createSound("Resources/broadcast.mp3", FMOD_DEFAULT, NULL, &broadcastSound);
		system->createSound("Resources/dm.mp3", FMOD_DEFAULT, NULL, &dmSound);
		system->createSound("Resources/server.mp3", FMOD_DEFAULT, NULL, &serverSound);
	}

	~Sound() {
		if (system != NULL) {
			system->close();
			system->release();
		}

		if (broadcastSound != NULL) broadcastSound->release();
		if (dmSound != NULL) dmSound->release();
		if (serverSound != NULL) serverSound->release();
	}

	void playBroadcastSound() { system->playSound(broadcastSound, NULL, false, &broadcastChannel); }
	void playDmSound() { system->playSound(dmSound, NULL, false, &dmChannel); }
	void playServerSound() { system->playSound(serverSound, NULL, false, &serverChannel); }
};