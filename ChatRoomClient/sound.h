#pragma once

#include <iostream>

#include <FMOD/fmod.hpp>
#include <FMOD/fmod_errors.h>

#pragma comment(lib, "fmod_vc.lib")

class Sound {
private:
	FMOD::Channel* channel = NULL;
	FMOD::Sound* broadcastSound = NULL;
	FMOD::Sound* dmSound = NULL;
	FMOD::Sound* serverSound = NULL;
	FMOD::System* system = NULL;
public:
	Sound() {
		FMOD::System_Create(&system);
		system->init(512, FMOD_INIT_NORMAL, NULL);
	}

	~Sound() {
		if (system != NULL) {
			system->close();
			system->release();
		}
	}

	void playBroadcastSound() {
		system->createSound("Resources/broadcast.mp3", FMOD_DEFAULT, NULL, &broadcastSound);
		system->playSound(broadcastSound, NULL, false, &channel);
		broadcastSound->release();
	}

	void playDmSound() {
		system->createSound("Resources/dm.mp3", FMOD_DEFAULT, NULL, &dmSound);
		system->playSound(dmSound, NULL, false, &channel);
		dmSound->release();
	}

	void playServerSound() {
		system->createSound("Resources/server.mp3", FMOD_DEFAULT, NULL, &serverSound);
		system->playSound(serverSound, NULL, false, &channel);
		serverSound->release();
	}
};