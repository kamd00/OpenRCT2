/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2/common.h>
#include <SDL2/SDL.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/core/String.hpp>
#include "../SDLException.h"
#include "AudioContext.h"

namespace OpenRCT2::Audio
{
    class AudioContext : public IAudioContext
    {
    private:
        IAudioMixer * _audioMixer = nullptr;

    public:
        AudioContext()
        {
            if (SDL_Init(SDL_INIT_AUDIO) < 0)
            {
                SDLException::Throw("SDL_Init(SDL_INIT_AUDIO)");
            }
            _audioMixer = AudioMixer::Create();
        }

        ~AudioContext() override
        {
            delete _audioMixer;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }

        IAudioMixer * GetMixer() override
        {
            return _audioMixer;
        }

        std::vector<std::string> GetOutputDevices() override
        {
            std::vector<std::string> devices;
            sint32 numDevices = SDL_GetNumAudioDevices(SDL_FALSE);
            for (sint32 i = 0; i < numDevices; i++)
            {
                std::string deviceName = String::ToStd(SDL_GetAudioDeviceName(i, SDL_FALSE));
                devices.push_back(deviceName);
            }
            return devices;
        }

        void SetOutputDevice(const std::string &deviceName) override
        {
            const char * szDeviceName = nullptr;
            if (!deviceName.empty())
            {
                szDeviceName = deviceName.c_str();
            }
            _audioMixer->Init(szDeviceName);
        }

        IAudioSource * CreateStreamFromWAV(const std::string &path) override
        {
            return AudioSource::CreateStreamFromWAV(path);
        }

        void StartTitleMusic() override { }

        IAudioChannel * PlaySound(sint32 soundId, sint32 volume, sint32 pan) override { return nullptr; }
        IAudioChannel * PlaySoundAtLocation(sint32 soundId, sint16 x, sint16 y, sint16 z) override { return nullptr; }
        IAudioChannel * PlaySoundPanned(sint32 soundId, sint32 pan, sint16 x, sint16 y, sint16 z) override { return nullptr; }

        void ToggleAllSounds() override { }
        void PauseSounds() override { }
        void UnpauseSounds() override { }

        void StopAll() override { }
        void StopCrowdSound() override { }
        void StopRainSound() override { }
        void StopRideMusic() override { }
        void StopTitleMusic() override { }
        void StopVehicleSounds() override { }
    };

    std::unique_ptr<IAudioContext> CreateAudioContext()
    {
        return std::make_unique<AudioContext>();
    }
} // namespace OpenRCT2::Audio
