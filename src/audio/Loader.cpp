#include "ILoader.hpp"
#include "WAV/WAVLoader.hpp"
//#include "OggVorbisLoader.hpp"

#include <iostream>
#include <string>
#include <fstream>

ILoader* GetLoaderForFile( const char* filePath )
{
	if ( std::ifstream( filePath ).fail() )
	{
		std::cout << "File doesn't exist: " << filePath << std::endl;
		return nullptr;
	}

	std::string pathString = filePath;
	size_t dotLocation = pathString.find_last_of( '.' ) + 1U;
	std::string fileFormat = pathString.substr( dotLocation );

	if ( fileFormat == "wav" )
	{
		std::cout << "WAV recognised" << std::endl;
		return new WAVLoader();
	}

	// In my AudioStuff project, I had Ogg Vorbis & Ogg Opus support,
	// however I was too lazy to merge that into here
	/*
	if ( fileFormat == "ogg" )
	{
		std::cout << "OGG recognised" << std::endl;
		return new OggVorbisLoader();
	}
	*/

	std::cout << "Unsupported file format: " << fileFormat << ", in " << filePath << std::endl;
	return nullptr;
}
