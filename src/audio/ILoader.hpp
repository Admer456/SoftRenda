#pragma once

#include <stdint.h>

class ILoader
{
public:
	virtual void		Load( const char* path ) = 0;
	virtual void		Dispose() = 0;
	virtual int8_t*		GetData() = 0;
	virtual size_t		GetLength() const = 0;
	virtual int32_t		GetSampleRate() const = 0;
	virtual int32_t		GetChannels() const = 0;
	virtual int32_t		GetBitsPerChannel() const = 0;
};

extern ILoader* GetLoaderForFile( const char* filePath );
