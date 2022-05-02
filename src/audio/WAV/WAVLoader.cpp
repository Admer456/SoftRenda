#define DR_WAV_IMPLEMENTATION 1
#include "dr_wav.h"
#include <iostream>

#include "../ILoader.hpp"
#include "WAVLoader.hpp"

void WAVLoader::Load( const char* path )
{
	drwav wav;
	if ( !drwav_init_file( &wav, path, nullptr ) )
	{
		std::cout << "Couldn't load file '" << path << "'" << std::endl;
		return;
	}

	numFrames = wav.totalPCMFrameCount;
	numSamplesPerSecond = wav.sampleRate;
	numChannels = wav.channels;
	numBitsPerChannel = wav.bitsPerSample;

	std::cout << "Num frames: " << numFrames << std::endl
		<< "Sample rate: " << numSamplesPerSecond << std::endl
		<< "Channels: " << numChannels << std::endl
		<< "Bit depth: " << numBitsPerChannel << std::endl
		<< "Length: " << ((float)numFrames / numSamplesPerSecond) << std::endl;

	const size_t bytesPerSample = numBitsPerChannel / 8U;
	const size_t size = numFrames * numChannels * bytesPerSample;
	data = new int8_t[size];

	drwav_read_pcm_frames( &wav, numFrames, data );
}

void WAVLoader::Dispose()
{
	if ( nullptr != data )
	{
		delete[] data;
		data = nullptr;
	}
}

int8_t* WAVLoader::GetData()
{
	return data;
}

size_t WAVLoader::GetLength() const
{
	return numFrames * numChannels * (numBitsPerChannel / 8);
}

int32_t WAVLoader::GetSampleRate() const
{
	return numSamplesPerSecond;
}

int32_t WAVLoader::GetChannels() const
{
	return numChannels;
}

int32_t WAVLoader::GetBitsPerChannel() const
{
	return numBitsPerChannel;
}
