
#pragma once

#include <cstdint>

namespace GoldBsp
{
	// CONSTANTS BEGIN

	constexpr uint32_t HalfLifeBspVersion = 30;
	// The GoldSRC BSP has 4 hulls:
	// 0: point hull
	// 1: player standing hull
	// 2: big monsters hull
	// 3: player crouching hull
	constexpr uint32_t MaxMapHulls = 4;
	// Mipmaps can subdivide only this far
	// It's the reason why the minimum tex resolution is 32x32 or so
	constexpr uint32_t MipLevels = 4;
	// Legacy Quake thing about ambient sounds inside a VIS area
	constexpr uint32_t NumAmbients = 4;
	// Maximum light styles per face
	constexpr uint32_t MaxLightmaps = 4;
	// Max BSP models
	constexpr uint32_t MaxMapModels = 512;
	// Max bytes for VIS data, 8 MB
	constexpr uint32_t MaxMapVisibilityData = 8 * 1024 * 1024;
	// Max bytes for lighting data, 48 MB
	constexpr uint32_t MaxMapLightingData = 48 * 1024 * 1024;
	// How many characters to store in the entity lump
	// This is arbitrarily chosen, 2 MB, should be enough,
	// even with very wordy keyvalues
	constexpr uint32_t MaxMapEntityData = 2048 * 1024;
	// Constant taken from VHLT's bspfile.h
	// The engine can only go up to 8192 leaves tho' (else entire leaves start disappearing)
	constexpr uint32_t MaxMapLeaves = 32760;
	constexpr uint32_t MaxEngineLeaves = 8192;
	// This value may be arbitrary, however the engine cannot do more than 32k planes
	// According to Vluzacn, MaxEnginePlanes should be 2x as big, because:
	// Faces can only use plane 0-32767, but clipnodes can use 0-65535
	constexpr uint32_t MaxMapPlanes = 256 * 1024;
	constexpr uint32_t MaxEnginePlanes = 32768;
	// Hard limit (vertices are stored as unsigned shorts in edges)
	constexpr uint32_t MaxMapVertices = 65535;
	// Hard limit (signed short, but the negative is used for contents)
	constexpr uint32_t MaxMapNodes = 32767;
	// face.textureInfo is a signed short, therefore MaxEngineTextureInfos is a hard limit
	// For a compiler's internal purposes though, there can be more than that
	constexpr uint32_t MaxMapTextureInfos = 262144;
	constexpr uint32_t MaxEngineTextureInfos = 32767;
	// According to Vluzacn: this ought to be 32k [referring to world faces], otherwise
	// faces in world can become invisible
	constexpr uint32_t MaxMapFaces = 65535;
	constexpr uint32_t MaxMapWorldFaces = 32768;
	// Hard limit (mark surfaces are stored as unsigned shorts)
	constexpr uint32_t MaxMapMarkSurfaces = 65535;
	// Our most beloved limit, right next to AllocBlock :)
	// Hard limit (signed short, but the negative is used for contents)
	constexpr uint32_t MaxMapClipnodes = 32767;
	// Arbitrary limitations
	constexpr uint32_t MaxMapEdges = 256000;
	constexpr uint32_t MaxMapSurfaceEdges = 512000;

	struct AmbientTypes
	{
		enum EnumType : int32_t
		{
			Water = 0,
			Sky,
			Slime,
			Lava
		};
	};

	// A plane's type is determined by the direction its normal is facing
	// This was typically used in Quake BSP compilers to determine the texture projection
	// axis. They would take the face normal, compare it to X, Y and Z direction vectors,
	// and see which one's the closest, so they could do a "world" UV projection accordingly.
	// That is, until Valve220 showed up...
	struct PlaneTypes
	{
		enum EnumType : int32_t
		{
			X,
			Y,
			Z,
			AnyX,
			AnyY,
			AnyZ
		};
	};

	struct Contents
	{
		enum EnumType : int32_t
		{
			// Air
			Empty = -1,
			// Inside of a solid brush
			Solid = -2,
			// Inside of a liquid
			Water = -3,
			Slime = -4,
			Lava = -5,
			// Inside of a sky brush
			Sky = -6,
			// Defines an origin for BSP models
			Origin = -7,

			// Unused current contents in Half-Life
			// Will push the player at very high velocities if touched
			Current0 = -9,
			Current90 = -10,
			Current180 = -11,
			Current270 = -12,

			// Non-solid but blocks VIS (todo: investigate more)
			Translucent = -15,
			// Filters down to CONTENTS_EMPTY by BSP. The engine should never see this
			Hint = -16,
			// Removed in CSG and BSP, VIS or RAD shouldn't have to deal with this, only clip planes!
			Null = -17,
			// Similar to CONTENTS_ORIGIN
			BoundingBox = -19,
			ToEmpty = -32,
		};
	};

	// CONSTANTS END

	struct BspLump
	{
		int32_t offset{ -1 };
		int32_t length{ -1 };

		enum EnumType : int32_t
		{
			Entities = 0,
			Planes,
			Textures,
			Vertices,
			Visibility,
			Nodes,
			TextureInfo,
			Faces,
			Lighting,
			Clipnodes,
			Leaves,
			MarkSurfaces,
			Edges,
			SurfaceEdges,
			Models,
			Max
		};
		static_assert(Max == 15, "Lump::MaxLumps must be 15");

		static const char* GetStringForLump( BspLump::EnumType lump );
	};

	// This is where we start from. After the correct BSP version is verified,
	// you simply copy the entire first 124 bytes of the BSP into this struct,
	// then jump to whatever lump you need using their offsets etc.
	// A BspHeader is initialised like so:
	// BspHeader* header = reinterpret_cast<BspHeader*>(rawFileData);
	struct BspHeader
	{
		enum ErrorCode
		{
			Error_Okay = 0,
			Error_LumpOutOfRange,
			Error_OddLumpSize,
			Error_DestinationNull
		};
		static const char* GetErrorString( BspHeader::ErrorCode errorCode );

		BspHeader() = default;
		BspHeader( uint8_t* data );

		// Sets outDestination to whatever data the lump is referring to
		// Returns true on success, false on failure
		ErrorCode SetLump( BspLump::EnumType lump, void*& outDdestination, int32_t size, int32_t& outLumpSize ) const;

		int32_t bspVersion{ -1 };
		BspLump lumps[BspLump::Max];
		uint8_t* rawData{ nullptr };

		constexpr static size_t BinarySize = sizeof( bspVersion ) + sizeof( lumps );
	};
	
	// Nothing to be said here. A plane is like an infinite surface in 3D space
	struct BspPlane
	{
		float normal[3];
		float distance;
		// PlaneTypes::X to PlaneTypes::AnyZ
		PlaneTypes::EnumType type;
	};

	// Reference to a single texture I think
	struct BspMipTextureLump
	{
		int32_t numMipTextures;
		int32_t dataOffsets[4];
	};

	// Basic texture information
	struct BspMipTexture
	{
		char name[16];
		uint32_t width;
		uint32_t height;
		uint32_t offsets[MipLevels];
	};

	// Texture projection info
	struct BspTextureInfo
	{
		// [s/t][xyz offset]
		float vectors[2][4];
		int32_t mipTexture;
		int32_t flags;
	};

	// A BSP vertex is nothing more than a vertex position
	// Texture coordinates etc. are stored elsewhere
	struct BspVertex
	{
		float point[3];
	};

	// A BSP model is basically what mappers call brush entities or solid entities.
	// It's its own little BSP tree which; however, doesn't undergo VIS
	struct BspModel
	{
		// Bounding box
		float mins[3];
		float maxs[3];
		// Position
		float origin[3];
		int32_t headNodeIndices[MaxMapHulls];
		int32_t visLeaves;
		int32_t firstFaceIndex;
		int32_t numFaces;
	};

	// You can think of a node as a subsection of 3D space in the BSP.
	// Nodes may have children which divide it even further. It may help
	// to imagine the BSP tree as a voxel octree and nodes as voxels in that octree
	// - even though that is technically incorrect - it helps you kinda visualise BSP nodes
	struct BspNode
	{
		int32_t planeIndex;
		// Negative numbers are -(leafs+1), not nodes
		int16_t children[2];
		// Bounding box of this node
		int16_t mins[3];
		int16_t maxs[3];
		uint16_t firstFaceIndex;
		uint16_t numFaces;
	};

	// Clipnodes are essentially collision data in a BSP
	// One clipnode is associated with a plane & 2 child clipnodes
	// Checking whether or not a point is inside some brush is done something like this:
	// 1. Grab a clipnode in the current node
	// 2. Grab the plane of that clipnode
	// 3. Check if above or below
	// 4. Repeat steps 2, 3 and 4 for children[0] and children[1] if possible
	//    until we reach the end and determine if the point is inside or outside
	// Visualising these clipnodes will require a recursive plane intersection algorithm
	// similar to what HLCSG does
	struct BspClipnode
	{
		int32_t planeIndex;
		// Negative values are contents
		int16_t children[2];
	};

	// BSP edges are simply 2 vertices
	struct BspEdge
	{
		uint16_t vertexIndices[2];
	};

	// A BSP face is a BSP plane, bound by a number of edges, which
	// is actually how we retrieve vertices for it
	// It also contains some lighting info
	struct BspFace
	{
		uint16_t planeIndex;
		int16_t sideIndex;

		int32_t firstEdgeIndex;
		int16_t numEdges;
		int16_t textureInfoIndex;

		// Lighting info
		uint8_t lightStyles[MaxLightmaps];
		// Start of [numStyles*surfaceSize] samples
		int32_t lightDataOffset;
	};

	// A BSP leaf is essentially a BSP node bound to a number of marksurfaces
	// Leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
	// All other leafs need visibility info
	struct BspLeaf
	{
		int32_t leafContents;
		// -1 = no visibility info
		int32_t visibilityOffset;

		// Bounding box for frustum culling
		int16_t mins[3];
		int16_t maxs[3];

		uint16_t firstMarkSurfaceIndex;
		uint16_t numMarkSurfaces;

		uint8_t ambientLevel[NumAmbients];
	};

	// All the BSP data
	struct BspMapData
	{
		BspMapData() = default;
		BspMapData( const BspHeader& header );
		~BspMapData();

		// Did we initialise stuff from the header?
		// Is everything valid?
		bool IsOkay() const;

		// [0-MaxMapModels]
		int32_t numModels{ -1 };
		BspModel* bspModels{ nullptr };

		// [0-MaxMapVisibilityData]
		int32_t visDataSize{ -1 };
		// VIS data is accessed directly, no casting
		uint8_t* visData{ nullptr };

		// You can access lightData using BSP faces:
		//	BspFace* face = GetFaceSomehow( bsp );
		//	uint8_t* lightSample = Bsp.lightData[face->lightDataOffset];

		// [0-MaxMapLightingData]
		int32_t lightDataSize{ -1 };
		// Lightmaps are stored as 128x128 images
		// Light data is accessed directly, no casting
		uint8_t* lightData{ nullptr };

		// Example usage of textureData:
		//	auto* textureLump = reinterpret_cast<BspMipTextureLump*>( Bsp.textureData );
		//	for ( int i = 0; i < textureLump->numMipTextures; i++ )
		//	{
		//		int offset = textureLump->dataOffset[i];
		//		int size = Bsp.textureDataSize - offset;
		//		if ( offset < 0 || size < sizeof( BspMipTexture ) )
		//		{
		//			continue; // Missing texture probably
		//		}
		//		
		//		// This point onwards, you can interact with the BspMipTexture
		//		auto* bspTexture = reinterpret_cast<BspMipTexture*>( &Bsp.textureData[offset] );

		// Cast textureData to BspMipTextureLump when you use it
		int32_t textureDataSize{ -1 };
		uint8_t *textureData{ nullptr };

		// [0-MaxMapEntityData]
		int32_t entityDataSize{ -1 };
		// Entity data is just one huge string
		char* entityData{ nullptr };

		// [0-MaxMapLeaves]
		int32_t numLeaves{ -1 };
		BspLeaf* bspLeaves{ nullptr };

		// [0-MaxMapPlanes]
		int32_t numPlanes{ -1 };
		BspPlane* bspPlanes{ nullptr };

		// [0-MaxMapVertices]
		int32_t numVertices{ -1 };
		BspVertex* bspVertices{ nullptr };

		// [0-MaxMapNodes]
		int32_t numNodes{ -1 };
		BspNode* bspNodes{ nullptr };

		// [0-MaxMapTextureInfos]
		int32_t numTextureInfos{ -1 };
		BspTextureInfo* bspTextureInfos{ nullptr };

		// [0-MaxMapFaces]
		int32_t numFaces{ -1 };
		BspFace* bspFaces{ nullptr };

		// [0-MaxMapClipnodes]
		int32_t numClipnodes{ -1 };
		BspClipnode* bspClipnodes{ nullptr };

		// [0-MaxMapEdges]
		int32_t numEdges{ -1 };
		BspEdge* bspEdges{ nullptr };

		// [0-MaxMapMarkSurfaces]
		int32_t numMarkSurfaces{ -1 };
		// Index into bspFaces
		uint16_t* markSurfaces{ nullptr };

		// [0-MaxMapSurfaceEdges]
		int32_t numSurfaceEdges{ -1 };
		// Index into bspEdges
		int32_t* surfaceEdges{ nullptr };
	};
}
