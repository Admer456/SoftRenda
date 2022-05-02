#pragma once

class WAVLoader final : public ILoader
{
public:
	void		Load( const char* path ) override;
	void		Dispose() override;
	int8_t*		GetData() override;
	size_t		GetLength() const override;
	int32_t		GetSampleRate() const override;
	int32_t		GetChannels() const override;
	int32_t		GetBitsPerChannel() const override;

private:
	int8_t*		data{ nullptr };
	size_t		numFrames{ 0 };
	int32_t		numSamplesPerSecond{ 0 };
	int32_t		numChannels{ 0 };
	int32_t		numBitsPerChannel{ 0 };
};
