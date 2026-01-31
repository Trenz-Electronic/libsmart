/*
 * WavFile.h
 *
 *  Created on: Dec 16, 2015
 *      Author: peeter
 */

#pragma once

#include "WavFile.h"

namespace smart {

class WavFileSimplePcm : protected WavFile
{
public:
	/** create simple wav file
	 *
	 * example:
	 * FILE *fp = fopen( "test.wav","wb" );
	 * WavFileSimplePcm thepcm( 2, 44100, 16 );
	 * uint32_t length = 2*2*44100;
	 * uint8_t *buf = thepcm.newData( length );
	 * for( int i=0; i<length; i++ ) buf[i] = 0; // fill in data
	 * thepcm.writeFile( fp );
	 */
	WavFileSimplePcm(
			uint16_t nchannels,	/// number of channels in file
			uint32_t samples_per_sec, /// sample rate
			uint16_t bits_per_sample, /// number of bits per sample of one channel
			uint32_t writing_factor = 1 /// how many times to reduce sampling rate when writing
	) : WavFile(),
	_riffchunk("WAVE"),
	_pcmchunk( &_riffchunk, nchannels, samples_per_sec / writing_factor, bits_per_sample ),
	_cuechunk( &_riffchunk ),
	_assocchunk( &_riffchunk ),
	_datachunk( &_riffchunk )
	{
		_datachunk.setSampleFactor(writing_factor,nchannels);
		_datachunk.setSampleWidth(bits_per_sample);
	};

	/// add cue point into file
	void addCuePoint( const char *name, uint32_t sample_offset, const char *description = 0 )
	{
		_cuechunk.setWavPoint(name,"data",sample_offset);
		if( description )
		{
			_labelchunks.push_back( std::make_shared<LabelChunk>( &_assocchunk,name,description) );
		}
	}

	/** add associated file into file
	 *
	 * arguments:
	 * name- the cue point name
	 * media- the media type
	 * file- pointer to the file
	 * file_size- size of the memory field and file
	 */
	void addAssocFile( const char *name, const char *media, const void *file, uint32_t file_size ){	_filechunks.push_back( std::make_shared<FileChunk>( &_assocchunk, name, media, file, file_size ) );}

	/// allocate memory for data field
	std::shared_ptr<MemBuffer> newData( uint32_t data_size ){	return _datachunk.addPiece( data_size ); }

	/// Add externally managed fraction into data chunk.
	/// Note: in the case of adding a null pointer, the user is responsible for writing the data chunk themselves.
	/// \param data	Pointer to the data, can be null.
	/// \param size Size of the data, in bytes.
	void addData( uint8_t *data, uint32_t data_size ){ _datachunk.addPiece( data, data_size );}

	/// get iterator for traversion of wave data
	std::shared_ptr<WavFile::PcmDataChunk::SampleIterator> getIterator(uint32_t index = 0){
		return _datachunk.getSampleIterator( (uint32_t)_pcmchunk.getPcmFormat()->waveFmt.wBlockAlign, index );
	}

	/// write the wav file
	uint32_t writeFile( FILE *fp ){	return _riffchunk.writeFile( fp ); }

	/// get number of samples in the file
	uint32_t getNumOfSamples(){
		auto pcm = _pcmchunk.getPcmFormat();
		if( !pcm )
			return 0;
		return _datachunk.getDataSize() / pcm->waveFmt.wBlockAlign;
	}

protected:
	RiffChunk _riffchunk;
	PcmChunk _pcmchunk;
	CueChunk _cuechunk;
	AssocListChunk _assocchunk;
	PcmDataChunk _datachunk;
	std::vector< std::shared_ptr<LabelChunk> > _labelchunks;
	std::vector< std::shared_ptr<FileChunk> > _filechunks;
};

} // namespace smart

