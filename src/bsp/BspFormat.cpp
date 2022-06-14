
#include <iostream>
#include <fstream>
#include "BspFormat.hpp"

using namespace GoldBsp;

const char* BspLump::GetStringForLump( BspLump::EnumType lump )
{
	switch ( lump )
	{
	case Entities: return "Entities";
	case Planes: return "Planes";
	case Textures: return "Textures";
	case Vertices: return "Vertices";
	case Visibility: return "Visibility";
	case Nodes: return "Nodes";
	case TextureInfo: return "TextureInfo";
	case Faces: return "Faces";
	case Lighting: return "Lighting";
	case Clipnodes: return "Clipnodes";
	case Leaves: return "Leaves";
	case MarkSurfaces: return "MarkSurfaces";
	case Edges: return "Edges";
	case SurfaceEdges: return "SurfaceEdges";
	case Models: return "Models";
	};

	return "Unknown Lump";
}

BspHeader::BspHeader( uint8_t* data )
	: rawData( data )
{
	bspVersion = *reinterpret_cast<int32_t*>( data );
	data += 4;

	for ( int i = 0; i < BspLump::Max; i++ )
	{
		lumps[i] = *reinterpret_cast<BspLump*>( data );
		data += sizeof( BspLump );
	}
}

BspHeader::ErrorCode BspHeader::SetLump( BspLump::EnumType lump, void*& destination, int32_t size, int32_t& outLumpSize ) const
{
	outLumpSize = -1;

	// Went overboard
	if ( lump >= BspLump::Max || lump < 0 )
	{
		return Error_LumpOutOfRange;
	}

	const int32_t& offset = lumps[lump].offset;
	const int32_t& length = lumps[lump].length;

	// Something does not quite match up
	if ( length % size )
	{
		return Error_OddLumpSize;
	}

	destination = rawData + offset;
	outLumpSize = length / size;

	return Error_Okay;
}

const char* BspHeader::GetErrorString( BspHeader::ErrorCode errorCode )
{
	switch ( errorCode )
	{
		case Error_Okay: return "okay";
		case Error_LumpOutOfRange: return "lump out of range";
		case Error_OddLumpSize: return "odd lump size";
		case Error_DestinationNull: return "destination is nullptr";
	};

	return "unknown error";
}

template<class T>
static bool AttemptCopyLump( const BspHeader& header, BspLump::EnumType lump, T*& destination, int32_t& outLumpSize )
{
	void* destinationToBeApplied;

	auto errorCode = header.SetLump( lump, destinationToBeApplied, sizeof(T), outLumpSize );
	if ( errorCode )
	{
		std::cout << "Error while copying the "
			<< BspLump::GetStringForLump( lump ) << " lump: "
			<< BspHeader::GetErrorString( errorCode ) << std::endl;

		destination = nullptr;
		return false;
	}

	destination = static_cast<T*>(destinationToBeApplied);
	return true;
}

BspMapData::BspMapData( const BspHeader& header )
{	
	using std::cout, std::endl;

	if ( header.bspVersion != HalfLifeBspVersion )
	{
		cout << "BSP is version " << header.bspVersion << " when I'm looking for " << HalfLifeBspVersion << endl;
		return;
	}

	// Excel spreadsheet moment
	if ( !AttemptCopyLump( header, BspLump::Models,       bspModels,       numModels       ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Vertices,     bspVertices,     numVertices     ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Planes,       bspPlanes,       numPlanes       ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Leaves,       bspLeaves,       numLeaves       ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Nodes,        bspNodes,        numNodes        ) ) return;
	if ( !AttemptCopyLump( header, BspLump::TextureInfo,  bspTextureInfos, numTextureInfos ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Clipnodes,    bspClipnodes,    numClipnodes    ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Faces,        bspFaces,        numFaces        ) ) return;
	if ( !AttemptCopyLump( header, BspLump::MarkSurfaces, markSurfaces,    numMarkSurfaces ) ) return;
	if ( !AttemptCopyLump( header, BspLump::SurfaceEdges, surfaceEdges,    numSurfaceEdges ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Edges,        bspEdges,        numEdges        ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Textures,     textureData,     textureDataSize ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Visibility,   visData,         visDataSize     ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Lighting,     lightData,       lightDataSize   ) ) return;
	if ( !AttemptCopyLump( header, BspLump::Entities,     entityData,      entityDataSize  ) ) return;
}

BspMapData::~BspMapData()
{
	bspModels = nullptr;
	bspVertices = nullptr;
	bspPlanes = nullptr;
	bspLeaves = nullptr;
	bspNodes = nullptr;
	bspTextureInfos = nullptr;
	bspClipnodes = nullptr;
	bspFaces = nullptr;
	markSurfaces = nullptr;
	surfaceEdges = nullptr;
	bspEdges = nullptr;
	textureData = nullptr;
	visData = nullptr;
	lightData = nullptr;
	entityData = nullptr;
}

bool BspMapData::IsOkay() const
{
	// The entity lump is the last one that gets copied to, so we can rely on it
	return (entityData != nullptr) && (entityDataSize > 0);
}
