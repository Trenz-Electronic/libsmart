/*
 * WavFile.h
 *
 *  Created on: Dec 16, 2015
 *      Author: peeter
 */

#pragma once

#include <map>

#include "WavFile.h"

namespace smart {

class WavFileDiskPcm : protected WavFile
{
public:
	/** create simple wav file based on data on the disk file
	 *
	 * example:
	 * WavFileDiskPcm thepcm( "/path/to/the/filename.wav" );
	 */
	WavFileDiskPcm( std::string filename ){
		_riffchunk = std::make_shared<RiffChunk>( filename, "WAVE" );
		do{
			if( test_create_chunk<PcmChunk>( _pcmchunk, &*_riffchunk ) )
			{
				_pcmchunk->seekFileEndOfChunk();
				continue;
			}
			if( test_create_chunk<CueChunk>( _cuechunk, &*_riffchunk ) )
			{
				_cuechunk->seekFileEndOfChunk();
				continue;
			}
			if( test_create_chunk<AssocListChunk>( _assocchunk, &*_riffchunk ) )
			{

				do
				{
					std::shared_ptr<LabelChunk> lchunk = nullptr;
					if( test_create_chunk<LabelChunk>( lchunk, &*_assocchunk) )
					{
						lchunk->seekFileEndOfChunk();
						label_chunk_t *hdr = lchunk->getLabelChunkHeader();
						std::string name( hdr->fccName.asChr, 4 );
						_labelchunks[name] = lchunk;
						continue;
					}
					std::shared_ptr<FileChunk> fchunk = nullptr;
					if( test_create_chunk<FileChunk>( fchunk, &*_assocchunk) )
					{
						fchunk->seekFileEndOfChunk();
						file_chunk_t *hdr = fchunk->getFileChunkHeader();
						std::string name( hdr->name.asChr, 4 );
						_filechunks[name] = fchunk;
						continue;
					}
					// file reading breaks if the file contains a chunk that is unknown,
					// use then generic chunk for getting around this issue.
					std::shared_ptr<Chunk> achunk = nullptr;
					if( test_create_chunk<Chunk>( achunk, &*_assocchunk ) )
					{
						achunk->seekFileEndOfChunk();
						continue;
					}
					break;
				}while( _assocchunk->inFileRange() );

				_assocchunk->seekFileEndOfChunk();
				continue;
			}
			if( test_create_chunk<PcmDataChunk>( _datachunk, &*_riffchunk ) )
			{
				_datachunk->seekFileEndOfChunk();
				continue;
			}
			// file reading breaks if the file contains a chunk that is unknown,
			// use then generic chunk for getting around this issue.
			std::shared_ptr<Chunk> achunk = nullptr;
			if( test_create_chunk<Chunk>( achunk, &*_riffchunk ) )
			{
				achunk->seekFileEndOfChunk();
				continue;
			}
			break;
		}while( _riffchunk->inFileRange() );
	}

	/** get pointer to associated file
	 *
	 * arguments:
	 * name- the cue point name
	 */
	MemBufferSptr getAssocFile( std::string name ){ return _filechunks[name]->getData(); }

	/** get pointer to associated label
	 *
	 * arguments:
	 * name- the cue point name
	 */
	MemBufferSptr getAssocLabel( std::string name ){ return _labelchunks[name]->getData(); }

	/** get bytes per sample */
	std::uint32_t getBytesPerSample(){
		return _pcmchunk->getPcmFormat()->waveFmt.wBlockAlign;
	}

	/** get number of samples */
	std::uint32_t getSampleCount(){
		return _datachunk->getDataSize() / getBytesPerSample();
	}

	/** get number of channels */
	std::uint32_t getNumOfChannels(){
		return _pcmchunk->getPcmFormat()->waveFmt.wChannels;
	}

	/** get sample rate */
	std::uint32_t getSampleRate(){
		return _pcmchunk->getPcmFormat()->waveFmt.dwSamplesPerSec;
	}

	/** get iterator for traversion of wave data */
	std::shared_ptr<WavFile::PcmDataChunk::SampleIterator> getIterator(uint32_t index = 0){
		return _datachunk->getSampleIterator( getBytesPerSample(), index );
	}

	/// Do we have data?
	bool hasData() const { return !!_datachunk; }
protected:
	template<class T>
	bool test_create_chunk( std::shared_ptr<T> &result, Chunk *parent )
	{
		if( result != nullptr )
			return false;
		result = std::make_shared<T>( parent );
		if( result->valid() )
			return true;
		result = nullptr;
		return false;
	}

protected:
	std::shared_ptr<RiffChunk> _riffchunk;
	std::shared_ptr<PcmChunk> _pcmchunk;
	std::shared_ptr<CueChunk> _cuechunk;
	std::shared_ptr<AssocListChunk> _assocchunk;
	std::shared_ptr<PcmDataChunk> _datachunk;
	std::map< std::string, std::shared_ptr<LabelChunk> > _labelchunks;
	std::map< std::string, std::shared_ptr<FileChunk> > _filechunks;
};

} // namespace smart

