/*
 * WavFile.cpp
 *
 *  Created on: Dec 16, 2015
 *      Author: peeter
 */

#include "WavFileSimple.h"

namespace smart {

WavFile::fourcc_t::fourcc_t( const char *init )
{
	asU32 = 0;
	for( int i=0; i<4; i++ )
	{
		if( *init )
		{
			asChr[i] = *init;
			init ++;
		}
		else
			asChr[i] = ' ';
	}
}


//---------------------------------------------------------------------------------------------------------

WavFile::Chunk::Chunk( Chunk *parent, uint32_t header_size, fourcc_t ckID )
{
	header_size = header_size < sizeof(chunk_t) ? sizeof(chunk_t) : header_size;
	_header = MemBuffer::make_shared(header_size);
	_min_size = sizeof(chunk_t);
	chunk_t *hdr = (chunk_t*)_header->_field;
	if( parent )
		_filebuf = parent->_filebuf;

	if( _filebuf != nullptr )
	{
		_filepos = ftell( _filebuf->_file );
		if( fread( hdr, header_size, 1, _filebuf->_file ) <= 0 )
		{
			hdr->ckID.asU32 = 0;
			hdr->ckSize = 0;
			seekFileStartOfChunk();
		}
		else if( ckID.asU32 && (hdr->ckID.asU32 != ckID.asU32) )
		{
			hdr->ckID.asU32 = 0;
			hdr->ckSize = 0;
			seekFileStartOfChunk();
		}
		else
		{
			if( parent )
				parent->setChild( this );
		}
	}
	else
	{
		hdr->ckID = ckID;
		hdr->ckSize = 0;
		if( parent )
			parent->setChild( this );
	}
}

WavFile::Chunk::Chunk( std::string fname, uint32_t header_size, fourcc_t ckID )
{
	header_size = header_size < sizeof(chunk_t) ? sizeof(chunk_t) : header_size;
	_header = MemBuffer::make_shared(header_size);
	_min_size = sizeof(chunk_t);
	_filebuf = FileBuffer::make_shared( fname );
	chunk_t *hdr = (chunk_t*)_header->_field;

	if( _filebuf != nullptr )
	{
		// file found, read data and verify
		_filepos = ftell( _filebuf->_file );
		if( fread( hdr, header_size, 1, _filebuf->_file ) <= 0 )
			_filebuf = nullptr;
		else if( hdr->ckID.asU32 != ckID.asU32 )
			_filebuf = nullptr; // destroy the file as the assumption failed, fall back
	}

	// if filebuf becomes nullptr, then it is like creating empty chunk above to fall back and avoid required user intervention
	if( _filebuf == nullptr )
	{
		hdr->ckID = ckID;
		hdr->ckSize = 0;
		// note the ckSize will be set when writing the data, after construction if this is 0- no file opened
		// some chunks do not have data fields, so they have ckSize always 0, but it will have no sense when the root is 0 size
	}
}


uint32_t WavFile::Chunk::getSize()
{
	uint32_t rv=0;
	rv += _header->_size;

	rv += getDataSize();

	// important assumption that the chunk must have data.
	if( rv <= _min_size )
		return 0;

	return rv;
};

uint32_t WavFile::Chunk::getDataSize()
{
	uint32_t rv=0;

	if( _filebuf != nullptr )
	{
		chunk_t *hdr = (chunk_t*)_header->_field;
		if( hdr->ckSize == 0 )
			if( _header->_size > sizeof(chunk_t) )
				rv = 0; // error case the ckSize must be bigger than 0
			else
				rv = sizeof(chunk_t);
		else if( hdr->ckSize + sizeof(chunk_t) < _header->_size )
			rv = 0; // incorrect ckSize, result would be very big number
		else
			rv = hdr->ckSize - (_header->_size - sizeof(chunk_t));
	}
	else if( _data.size() )
	{
		// leaf chunk
		for( auto i = _data.begin(); i != _data.end(); i++ )
		{
			rv += (*i)->_size;
		}
	}
	else for( auto i = _contents.begin(); i != _contents.end(); i++ )
	{
		// compound chunk
		rv += (*i)->getSize();
	}

	return rv;
};


uint32_t WavFile::Chunk::fillBuffer( uint8_t **buf, uint32_t &maxlen )
{
	uint32_t rv = getSize();

	// important assumption that the chunk must have data.
	if( !rv  )
		return 0;

	// the data must fit into buffer
	if( maxlen < rv )
		return 0;

	// TODO: add support for writing from file
	if( _filebuf != nullptr )
		return 0;

	chunk_t *hdr = (chunk_t*)_header->_field;
	hdr->ckSize = rv - sizeof(chunk_t); // the chunk size does not contain chunk_t header bytes

	if( *buf != _header->_field )
		memcpy( *buf, _header->_field, _header->_size );
	*buf += _header->_size; maxlen -= _header->_size;
	rv = _header->_size;

	if( _data.size() )
	{
		for( auto i = _data.begin(); i != _data.end(); i++ )
		{
			if( *buf != (*i)->_field )
				memcpy( *buf, (*i)->_field, (*i)->_size );
			*buf += (*i)->_size; maxlen -= (*i)->_size;
			rv += (*i)->_size;
		}
	}
	else for( auto i = _contents.begin(); i != _contents.end(); i++ )
	{
		rv += (*i)->fillBuffer( buf, maxlen );
	}

	return rv;
}

uint32_t WavFile::Chunk::writeFile( FILE *fp )
{
	if( fp == NULL )
		return 0;

	uint32_t rv = getSize();

	// important assumption that the chunk must have data.
	if( !rv  )
		return 0;

	// TODO: add support for writing from file
	if( _filebuf != nullptr )
		return 0;


	chunk_t *hdr = (chunk_t*)_header->_field;
	hdr->ckSize = rv - sizeof(chunk_t); // the chunk size does not contain chunk_t header bytes

	rv = fwrite( _header->_field, 1, _header->_size, fp );

	if( _data.size() )
	{
		for (auto i : _data)
		{
			if (i->_field != nullptr)
			{
				rv += fwrite(i->_field, 1, i->_size, fp);
			}
		}
	}
	else for( auto i = _contents.begin(); i != _contents.end(); i++ )
	{
		rv += (*i)->writeFile( fp );
	}

	return rv;
}

void WavFile::Chunk::setChild( Chunk *child )
{
	if( _data.size() )
		return;	// this is leaf chunk

	_contents.push_back( child );
	child->_filebuf = _filebuf; // all children use the same file
};

std::shared_ptr<MemBuffer> WavFile::Chunk::getData( uint32_t i )
{
	if( _filebuf != nullptr )
	{
		// at the moment, onlu i==0 is supported
		i = 0;
		if( _data.size() )
			return _data[0];

		// read the data from disk
		chunk_t *hdr = (chunk_t*)_header->_field;
		if( hdr->ckSize == 0 )
			return MemBuffer::make_shared(0,0); // chunk with no data
		if( hdr->ckID.asU32 == 0 )
			return MemBuffer::make_shared(0,0); // errant chunk

		// create the buffer into memory and read data in
		auto bf = MemBuffer::make_shared( getDataSize() );
		seekFileStartOfData();
		fread( bf->_field, bf->_size, 1, _filebuf->_file );
		_data.push_back( bf );
	}

	if( _data.size() > i )
	{
		return _data[i];
	}

	// default return empty buffer
	return MemBuffer::make_shared(0,0);
};


void WavFile::Chunk::addPiece(
		uint8_t *origin,
		uint32_t size )
{
	if( _contents.size() )
		return; // this is not leaf chunk

	// TODO: add support for adding additional data to existing file
	if( _filebuf != nullptr )
		return;

	addPiece( MemBuffer::make_shared(origin,size) );
}

std::shared_ptr<MemBuffer> WavFile::Chunk::addPiece( uint32_t size )
{
	if( _contents.size() )
		return MemBuffer::make_shared(0,0);
	if( size == 0 )
		return MemBuffer::make_shared(0,0);
	// TODO: add support for adding additional data to existing file
	if( _filebuf != nullptr )
		return MemBuffer::make_shared(0,0);

	std::shared_ptr<MemBuffer> rv = MemBuffer::make_shared( size );

	addPiece( rv );

	return rv;
}

void WavFile::Chunk::addPiece( std::shared_ptr<MemBuffer> buf )
{
	if( _contents.size() )
		return; // this is not leaf chunk
	// TODO: add support for adding additional data to existing file
	if( _filebuf != nullptr )
		return;

	_data.push_back( buf );
}

void WavFile::Chunk::seekFileEndOfChunk()
{
	if( _filebuf == nullptr )
		return;

	int64_t pos = _filepos + getDataSize() + _header->_size;
	fseek( _filebuf->_file, static_cast<long>(pos), SEEK_SET );
}

/** seek file to start of data of chunk
 *
 */
void WavFile::Chunk::seekFileStartOfData( int64_t dataseek )
{
	if( _filebuf == nullptr )
		return;

	int64_t pos = _filepos + _header->_size + dataseek;
	fseek( _filebuf->_file, static_cast<long>(pos), SEEK_SET );
}

void WavFile::Chunk::seekFileStartOfChunk()
{
	if( _filebuf == nullptr )
		return;

	int64_t pos = _filepos;
	fseek( _filebuf->_file, static_cast<long>(pos), SEEK_SET );
}

bool WavFile::Chunk::inFileRange()
{
	if( _filebuf == nullptr )
		return false;

	int64_t pos = ftell( _filebuf->_file );

	if( pos < _filepos )
		return false;

	chunk_t *hdr = (chunk_t*)_header->_field;
	if( pos >= (_filepos + hdr->ckSize + sizeof(chunk_t)) )
		return false;

	return true;
}

//---------------------------------------------------------------------------------------------------------
WavFile::LeafChunk::LeafChunk( Chunk *parent, uint32_t header_size, uint32_t data_size, fourcc_t ckID )
: Chunk( parent, header_size, ckID )
{
  // align the data size, 
  // reason: strage behaviour of frite, trhows alignment trap on big file
	uint32_t remainder = data_size % 4;
	if( remainder )
		data_size += 4 - remainder;
  // allocate memory for the data 
	if( data_size > 0 )
		addPiece( data_size );
}

//---------------------------------------------------------------------------------------------------------

WavFile::RiffChunk::RiffChunk( fourcc_t formType )
: Chunk( 0, sizeof(riff_chunk_t), "RIFF" )
{
	riff_chunk_t *hdr = (riff_chunk_t*)_header->_field;
	_min_size = sizeof(riff_chunk_t);

	hdr->formType = formType;
}

WavFile::RiffChunk::RiffChunk( std::string fname, fourcc_t formType )
: Chunk( fname, sizeof(riff_chunk_t), "RIFF" )
{
	riff_chunk_t *hdr = (riff_chunk_t*)_header->_field;
	_min_size = sizeof(riff_chunk_t);

	// verify if the file is in correct form
	if( hdr->formType.asU32 != formType.asU32 )
	{
		// form error
		_filebuf = nullptr;
		hdr->riff.ckSize = 0;
	}
}

//---------------------------------------------------------------------------------------------------------

WavFile::WaveChunk::WaveChunk( Chunk *parent, uint32_t header_size,
		uint16_t channels,
		uint32_t samples_per_sec,
		uint16_t bytes_per_value
): Chunk( parent, header_size, "fmt ")
{
	wave_format_t *hdr = (wave_format_t*)_header->_field;

	hdr->wFormatTag = 0; // the category is initiatet to 0
	hdr->wChannels = channels;
	hdr->dwSamplesPerSec = samples_per_sec;
	hdr->dwAvgBytesPerSec = samples_per_sec * channels * bytes_per_value;
	hdr->wBlockAlign = channels * bytes_per_value;
}

WavFile::WaveChunk::WaveChunk( Chunk *parent ): Chunk( parent, sizeof(pcm_format_t), "fmt ")
{
}

WavFile::PcmChunk::PcmChunk( Chunk *parent,
		uint16_t channels,
		uint32_t samples_per_sec,
		uint16_t bits_per_sample
): WaveChunk( parent, sizeof(pcm_format_t), channels, samples_per_sec, (bits_per_sample+7)/8)
{
	pcm_format_t *hdr =(pcm_format_t*)_header->_field;

	hdr->wBitsPerSample = bits_per_sample;
	hdr->waveFmt.wFormatTag = 1;
}

WavFile::PcmChunk::PcmChunk( Chunk *parent ):
		WaveChunk( parent )
{
}


//---------------------------------------------------------------------------------------------------------

void WavFile::PcmDataChunk::setSampleWidth(unsigned int widthInBits)
{
	if (_nchannels == 0u) {
		return;
	}

	const unsigned int	nbits = ((widthInBits + 7u) / 8u) * 8u;
	_row_length = ((nbits * _nchannels + 31) /32) * 4;
}

uint32_t WavFile::PcmDataChunk::writeFile( FILE *fp )
{
	uint32_t rv = 0;
	if( _ratefactor > 1 )
	{
		// generic chunk header part
		if( fp == NULL )
			return 0;

		rv = getSize();

		// important assumption that the chunk must have data.
		if( !rv  )
			return 0;

		chunk_t *hdr = (chunk_t*)_header->_field;
		hdr->ckSize = rv - sizeof(chunk_t); // the chunk size does not contain chunk_t header bytes

		rv = fwrite( _header->_field, 1, _header->_size, fp );

		// custom data part
		int16_t *sample;
		int32_t samplelen = _nchannels*sizeof(int16_t);
		auto obuf = MemBuffer::make_shared( 1024*samplelen );
		int16_t *pobuf = (int16_t*)obuf->_field;
		int16_t *end = (int16_t*)((int8_t*)(obuf->_field)+obuf->_size);
		auto sit = getSampleIterator(samplelen);
		while( (sample=(int16_t*)sit->getSampleInc( samplelen, _ratefactor)->_field) != 0 )
		{
			memcpy( pobuf, sample, samplelen );
			pobuf += _nchannels;
			if( pobuf >= end )
			{
				// temporary buffer is full, write the file
				pobuf = (int16_t*)obuf->_field;
				rv += fwrite( obuf->_field, 1, obuf->_size, fp );
			}
		}
		// write the remaining of the not full buffer
		size_t remains = (size_t)pobuf - (size_t)(obuf->_field);
		if( remains > 0 )
		{
			rv += fwrite( obuf->_field, 1, remains, fp );
		}
	}
	else
	{
		rv = Chunk::writeFile( fp );
	}
	//printf("WavFile::PcmDataChunk::writeFile rv=%u\n", rv);
	return rv;
}

uint32_t WavFile::PcmDataChunk::getDataSize()
{
	const unsigned int	data_size = Chunk::getDataSize() / _ratefactor;
	if (_row_length > 0u) {
		const unsigned int	real_data_size = (data_size / _row_length) * _row_length;
		return real_data_size;
	}
	else {
		return data_size;
	}
}

/**
 * THE INTERFACE SAMPLE ITERATOR
 */

WavFile::PcmDataChunk::SampleIterator::SampleIterator( PcmDataChunk *chunk, uint32_t len )
{
	_chunk = chunk;
	_samplelen = len;
}

#if (1) //SampleIteratorFile .................................................................................

/**
 * sample iterator that iterates inside file on disk
 *
 */
class SampleIteratorFile : public WavFile::PcmDataChunk::SampleIterator {
public:
	/**
	 * Important assumption: each buffer in data divides exactly with len
	 */
	SampleIteratorFile( WavFile::PcmDataChunk *chunk, uint32_t len, uint32_t index = 0, uint32_t fraction = 0 );
	/** get single sample from the data
	 * return:
	 * pointer to the sample of all channels, 0 if the sample does not exist
	 */
	virtual MemBufferSptr getSample( uint32_t count = 1 );
	/** set iterator position
	 * return:
	 * pointer to this iterator
	 */
	virtual SampleIterator *setPos( uint32_t index, uint32_t fraction = 0 );
	virtual SampleIterator *nextPos( uint32_t index=1, uint32_t fraction = 0 );
	/** get single sample from the data and increase the iterator afterwards
	 * return:
	 * pointer to the sample of all channels, 0 if the sample does not exist
	 */
	virtual MemBufferSptr getSampleInc( uint32_t count = 1, uint32_t index=1, uint32_t fraction = 0 );
protected:
	int64_t _cursor;
};

WavFile::PcmDataChunk::SampleIterator *SampleIteratorFile::setPos( uint32_t index, uint32_t fraction )
{
	_cursor = (int64_t)_samplelen * index;
	_fraction = fraction;
	return this;
}

WavFile::PcmDataChunk::SampleIterator *SampleIteratorFile::nextPos( uint32_t index, uint32_t fraction )
{
	_fraction += fraction;
	uint32_t f = fraction >> FIX;
	_fraction &= (1<<FIX)-1;
	_cursor += (int64_t)_samplelen * (index+f);
	return this;
}


SampleIteratorFile::SampleIteratorFile( WavFile::PcmDataChunk *chunk, uint32_t len, uint32_t index, uint32_t fraction ):
		SampleIterator( chunk, len )
{
	setPos(index,fraction);
}

MemBufferSptr SampleIteratorFile::getSample( uint32_t count )
{
	_chunk->seekFileStartOfData( _cursor );
	MemBufferSptr rv = MemBuffer::make_shared( count * _samplelen );
	fread( rv->_field, _samplelen, count, _chunk->_filebuf->_file );
	return rv;
}

MemBufferSptr SampleIteratorFile::getSampleInc( uint32_t count , uint32_t index, uint32_t fraction )
{
	MemBufferSptr rv = getSample( count );
	nextPos( index, fraction );
	return rv;
}

#endif //SampleIteratorFile

#if (1) //SampleIteratorMemory .............................................................................

/**
 * sample iterator that iterates inside memory
 *
 */
class SampleIteratorMemory : public WavFile::PcmDataChunk::SampleIterator {
public:
	/**
	 * Important assumption: each buffer in data divides exactly with len
	 *
	 * chunk - the chunk where to iterate
	 * len - sample len in bytes
	 * index - starting index of the sample
	 * fraction - starting fraction of the sample index
	 */
	SampleIteratorMemory( WavFile::PcmDataChunk *chunk, uint32_t len, uint32_t index = 0, uint32_t fraction = 0 );
	/** get single sample from the data
	 * return:
	 * pointer to the sample of all channels, 0 if the sample does not exist
	 */
	virtual MemBufferSptr getSample( uint32_t count = 1 );
	/** set iterator position
	 * return:
	 * pointer to this iterator
	 */
	virtual SampleIterator *setPos( uint32_t index, uint32_t fraction = 0 );
	virtual SampleIterator *nextPos( uint32_t index=1, uint32_t fraction = 0 );
	/** get single sample from the data and increase the iterator afterwards
	 * return:
	 * pointer to the sample of all channels, 0 if the sample does not exist
	 */
	virtual MemBufferSptr getSampleInc( uint32_t count = 1, uint32_t index=1, uint32_t fraction = 0 );
protected:
	/// Index to the data junk
	uint32_t	_junkid;
	/// Offset from the junk
	uint8_t		*_junkoffset;
	/// Buffer end
	uint8_t		*_junkend;
};

SampleIteratorMemory::SampleIteratorMemory( WavFile::PcmDataChunk *chunk, uint32_t len, uint32_t index, uint32_t fraction ):
		SampleIterator( chunk, len )
{
	_junkid = 0xFFffFFff;
	_junkoffset = 0;
	_junkend = 0;
	setPos( index, fraction );
}

WavFile::PcmDataChunk::SampleIterator*
SampleIteratorMemory::setPos( uint32_t index, uint32_t fraction )
{
	_fraction = fraction;
	index *= _samplelen;
	_junkid = 0xFFffFFff; // invalidate iterator for the case we will not find the index location
	for ( unsigned int i=0; i<_chunk->_data.size(); i++ )
	{
		if( _chunk->_data[i]->_size < index )
		{
			// note error will happen if _size do not divide with _samplelen
			index -= _chunk->_data[i]->_size;
		}
		else if( (index+_samplelen) <= _chunk->_data[i]->_size ) // protect against buffer overrun
		{
			_junkid = i;
			_junkoffset = (uint8_t*)(_chunk->_data[_junkid]->_field) + index;
			_junkend = (uint8_t*)(_chunk->_data[_junkid]->_field) + _chunk->_data[_junkid]->_size;
			break;
		}
		else
			break; // TODO: fatal error, should throw exception instead, the buffer size must be incorrect
		}
	return this;
}

WavFile::PcmDataChunk::SampleIterator*
SampleIteratorMemory::nextPos( uint32_t index, uint32_t fraction )
{
	if( _junkid >= _chunk->_data.size() )
	{
		_junkid = 0xFFffFFff;
		_junkoffset = 0;
		_junkend = 0;
		return this;
	}

	_fraction += fraction;
	uint32_t f = fraction >> FIX;
	_fraction &= (1<<FIX)-1;
	_junkoffset += _samplelen * (index+f);

	while( _junkoffset >= _junkend )
	{
		_junkid++;
		if(  _junkid >= _chunk->_data.size() )
		{
			_junkid = 0xFFffFFff;
			_junkoffset = 0;
			_junkend = 0;
			return this;
		}
		_junkoffset = ((uint8_t*)_chunk->_data[_junkid]->_field) + ((size_t)_junkoffset - (size_t)_junkend);
		_junkend = ((uint8_t*)_chunk->_data[_junkid]->_field) + _chunk->_data[_junkid]->_size;
	}

	if( (_junkoffset+_samplelen) <= _junkend )
	{
		// returns only if offset and all the content fits into buffer
		return this;
	}

	_junkid++;
	if( _junkid < _chunk->_data.size() )
	{
		//printf("next buffer\n");
		_junkoffset = (uint8_t*)(_chunk->_data[_junkid]->_field);
		_junkend = (uint8_t*)(_chunk->_data[_junkid]->_field) + _chunk->_data[_junkid]->_size;
		return this;
	}
	// no more data
	//printf("No more data _junkid=%u, _chunk->_data.size()=%u\n ", _junkid, _chunk->_data.size() );
	_junkid = 0xFFffFFff;
	_junkoffset = 0;
	_junkend = 0;
	return this;
}

MemBufferSptr SampleIteratorMemory::getSample( uint32_t count )
{
	if( _junkid == 0xFFffFFff )
		return MemBuffer::make_shared(0,0);
	//FIXME: the _samplelen * count can leap over the buffer boundary, new buffer shall be made
	return MemBuffer::make_shared( _chunk->_data[_junkid], _junkoffset, _samplelen * count );
}

MemBufferSptr SampleIteratorMemory::getSampleInc( uint32_t count, uint32_t index, uint32_t fraction )
{
	auto rv = getSample( count );
	nextPos( index, fraction );
	return rv;
}

#endif //SampleIteratorMemory

/**
 * Get the proper sample iterator
 */
std::shared_ptr<WavFile::PcmDataChunk::SampleIterator>
WavFile::PcmDataChunk::getSampleIterator( uint32_t len, uint32_t index, uint32_t fraction )
{
	if( _filebuf != nullptr )
	{
		return std::make_shared<SampleIteratorFile>( this, len, index, fraction );
	}
	return std::make_shared<SampleIteratorMemory>( this, len, index, fraction );
}


//---------------------------------------------------------------------------------------------------------

WavFile::CueChunk::CueChunk( Chunk *parent )
: Chunk( parent, sizeof(cue_chunk_t), "cue ")
{
	cue_chunk_t *hdr = (cue_chunk_t*)_header->_field;
	_min_size = sizeof(cue_chunk_t);

	hdr->dwCuePoints = 0;
}

void WavFile::CueChunk::setPoint( fourcc_t name,
		fourcc_t chunk_name,
		uint32_t chunk_start,
		uint32_t block_start,
		uint32_t sample_offset
)
{
	cue_chunk_t *hdr = (cue_chunk_t*)_header->_field;

	cue_point_t *point = (cue_point_t *)addPiece( sizeof(cue_point_t) )->_field;

	point->dwName = name;
	point->dwPosition = hdr->dwCuePoints;
	point->fccChunk = chunk_name;
	point->dwChunkStart = chunk_start;
	point->dwBlockStart = block_start;
	point->dwSampleOffset = sample_offset;

	hdr->dwCuePoints++;
}

/// set point of single data chunk wav file
void WavFile::CueChunk::setWavPoint( const char* name,
		const char* chunk_name, uint32_t sample_offset )
{
	setPoint( name,
			chunk_name, 0, 0, sample_offset );
}

//---------------------------------------------------------------------------------------------------------

WavFile::AssocListChunk::AssocListChunk( Chunk *parent ) : Chunk( parent, sizeof(assoc_data_t), "LIST" )
{
	assoc_data_t *hdr = (assoc_data_t*)_header->_field;
	_min_size = sizeof(assoc_data_t);

	hdr->fccAdtl = fourcc_t("adtl");
}

//---------------------------------------------------------------------------------------------------------

WavFile::LabelChunk::LabelChunk( Chunk *parent, fourcc_t name, const char *label )
: LeafChunk( parent, sizeof(label_chunk_t), strlen(label)+1, "labl" )
{
	label_chunk_t *hdr =(label_chunk_t*)_header->_field;
	_min_size = sizeof(label_chunk_t);

	hdr->fccName = name;
	strncpy( (char*)_data[0]->_field, (const char*)label, _data[0]->_size );
}

//---------------------------------------------------------------------------------------------------------

WavFile::FileChunk::FileChunk( Chunk *parent, fourcc_t name, fourcc_t media, const void *file, uint32_t file_size ) :
	LeafChunk( parent, sizeof(file_chunk_t), file_size, "file" )
{
	file_chunk_t *hdr = (file_chunk_t*)_header->_field;
	_min_size = sizeof(file_chunk_t);

	hdr->name = name;
	hdr->media = media;

	memcpy( _data[0]->_field, file, file_size );
}

} // namespace smart
