#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <assert.h>
#include <string>
#include <errno.h>

struct IndexHeader
{
	char		m_magic[ 4 ];		// "PACK"
	unsigned	m_unknown0;
	unsigned	m_unknown1;
	unsigned	m_packedFileMaxID;	// packed file num - 1
	unsigned	m_lumpFileMaxSize;
	unsigned	m_lumpFileNum;
	char		m_align[ 230 ];
};

struct IndexLumpDesc
{
	unsigned	m_unknown;
	unsigned	m_lumpSize;
	unsigned char	m_lumpPartID;
	unsigned char	m_lumpID;
	char		m_align[ 2 ];
};

struct IndexFileDesc
{
	char		m_filename[ 120 ];
	unsigned	m_offset;
	unsigned	m_size;
};

IndexHeader	gIndexHeader;
IndexLumpDesc*	gIndexLumpDescArr = NULL;
IndexFileDesc*	gIndexFileDescArr = NULL;


unsigned AlignUp( unsigned val, unsigned alignment )
{
	return ( ( val - 1 ) & ~( alignment - 1 ) ) + alignment;
}

void CreateFilePath( char path[ 1024 ] )
{
	unsigned const pathLen = strlen( path );
	for ( unsigned i = 0; i < pathLen; ++i )
	{		
		if ( path[ i ] == '/' )
		{
			path[ i ] = 0;
			int ret = _mkdir( path );
			assert( ret != ENOENT );
			path[ i ] = '/';
		}
	}
}

unsigned FileRead( FILE* f, int *seed, char* dstBuf, unsigned readSize )
{
	assert( f );

	unsigned const readByteNum = fread( dstBuf, 1, readSize, f );
	for ( unsigned i = 0; i < readByteNum; ++i )
	{
		int const xorKey = *seed * ( *seed * *seed * 0x73 - 0x1B ) + 0x0D;
		dstBuf[ i ] ^= xorKey;
		++*seed;
	}

	return readByteNum;
}


bool ReadIndexLump( char const* srcDir )
{
	char fileName[ 1024 ];
	sprintf( fileName, "%s%s", srcDir, "lump.idx" );
	FILE* file = fopen( fileName, "rb" );
	if ( !file )
	{
		printf( "ERROR: Can't open %s", fileName );
		return false;
	}

	fseek( file, 0, SEEK_END );
	unsigned const fileSize = ftell( file );
	fseek( file, 0, SEEK_SET );

	int seed = fileSize + 0x006FD37D;
	FileRead( file, &seed, (char*) &gIndexHeader, sizeof( gIndexHeader ) );

	gIndexLumpDescArr = new IndexLumpDesc[ gIndexHeader.m_lumpFileNum ];
	for ( unsigned i = 0; i < gIndexHeader.m_lumpFileNum; ++i )
	{
		FileRead( file, &seed, (char*) &gIndexLumpDescArr[ i ], sizeof( gIndexLumpDescArr[ i ] ) );
	}

	gIndexFileDescArr = new IndexFileDesc[ gIndexHeader.m_packedFileMaxID + 1 ];
	for ( unsigned i = 0; i < gIndexHeader.m_packedFileMaxID + 1; ++i )
	{
		FileRead( file, &seed, (char*) &gIndexFileDescArr[ i ], sizeof( gIndexFileDescArr[ i ] ) );
	}

	fclose( file );
	return true;
}

bool ExtractFiles( char const* srcDir, char const* dstDir )
{
	unsigned	fileOffset	= 0;
	int		indexFileDescID	= -1;
	unsigned	writeLeft	= 0;
	FILE*		fileDst		= NULL;
	for ( unsigned iLumpFile = 0; iLumpFile < gIndexHeader.m_lumpFileNum; ++iLumpFile )
	{
		IndexLumpDesc& lumpDesc = gIndexLumpDescArr[ iLumpFile ];

		char fileNameSrc[ 1024 ];
		if ( lumpDesc.m_lumpPartID == 0 )
		{
			sprintf( fileNameSrc, "%slump_%d.pak", srcDir, lumpDesc.m_lumpID );
		}
		else
		{
			sprintf( fileNameSrc, "%slump_%d_%d.pak", srcDir, lumpDesc.m_lumpID, lumpDesc.m_lumpPartID );
		}


		FILE* fileSrc = fopen( fileNameSrc, "rb" );
		if ( !fileSrc )
		{
			printf( "ERROR: Can't open: %s", fileNameSrc );
			return false;
		}

		int seed = lumpDesc.m_lumpSize + 0x006FD37D;
		while( !feof( fileSrc ) )
		{
			char tempBuf[ 64 * 1024 ];
			unsigned const tempBufSize = FileRead( fileSrc, &seed, tempBuf, sizeof( tempBuf ) );

			unsigned tempBufOffset = 0;
			while ( tempBufOffset < tempBufSize )
			{
				if ( !fileDst )
				{
					++indexFileDescID;
					IndexFileDesc const& fileDesc = gIndexFileDescArr[ indexFileDescID ];

					char fileNameDst[ 1024 ];
					sprintf( fileNameDst, "%s%s", dstDir, fileDesc.m_filename );
					CreateFilePath( fileNameDst );

					fileDst = fopen( fileNameDst, "wb" );
					printf( "Extracting: %s\n", fileNameDst );
					if ( !fileDst )
					{
						printf( "Error: can't create output file: %s", fileNameDst );
						return 1;
					}
					writeLeft = fileDesc.m_size;
				}

				unsigned const writeSize = std::min( tempBufSize - tempBufOffset, writeLeft );
				fwrite( tempBuf + tempBufOffset, 1, writeSize, fileDst );
				tempBufOffset	+= writeSize;
				writeLeft	-= writeSize;

				if ( writeLeft == 0 )
				{
					tempBufOffset = AlignUp( tempBufOffset, 128 );
					fclose( fileDst );
					fileDst = NULL;
				}
			}
		}

		fclose( fileSrc );
	}

	return true;
}

int main( int argc, char* argv[] )
{
	printf( "PixelJunk Eden w32 data extractor\n" );
	printf( "Usage: extEden.exe -s srcDir/ -d dstDir/\n" );

	char const* srcDir = "";
	char const* dstDir = "extracted/";
	for ( int i = 0; i < argc; ++i )
	{
		if ( strcmp( argv[ i ], "-s" ) == 0 && i + 1 < argc )
		{
			srcDir = argv[ i + 1 ];
		}

		if ( strcmp( argv[ i ], "-d" ) == 0 && i + 1 < argc )
		{
			dstDir = argv[ i + 1 ];
		}
	}

	if ( ReadIndexLump( srcDir ) )
	{
		ExtractFiles( srcDir, dstDir );
	}

	delete[] gIndexLumpDescArr;
	delete[] gIndexFileDescArr;
	return 0;
}