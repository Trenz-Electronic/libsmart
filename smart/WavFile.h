/*
 * WavFile.h
 *
 *  Created on: Dec 16, 2015
 *      Author: peeter
 */

#pragma once


#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <string>	// std::string
#include <memory>
#include <vector>

#include <stdio.h>

#include "MemBuffer.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4200)
#endif

namespace smart {

class WavFile
{
public:
	WavFile(){};
	~WavFile(){};
public:

	/// File buffer handling
	class FileBuffer
	{
	public:
		static std::shared_ptr<FileBuffer> make_shared( std::string fname ){
			auto rv = std::shared_ptr<FileBuffer>( new FileBuffer(fname) );
			if( rv->_file )
				return rv;
			return nullptr;
		};
	protected:
		/// initialise file buffer
		FileBuffer( std::string fname ){ _file = fopen( fname.c_str(), "r+b" ); };
	public:
		virtual ~FileBuffer(){ if(_file) fclose(_file); };
		FILE *_file;
	};

#pragma pack(push, 1)
	/// the fourcharacter type
	union fourcc_t
	{
		char		asChr[4];		/// the field as 4 characters
		uint32_t	asU32;		/// as 32 bit number

		fourcc_t( uint32_t init ){ asU32 = init; }
		fourcc_t( int32_t init ){ asU32 = (uint32_t)init; }
		fourcc_t( const char *init );
	};

	struct chunk_header_t {
		fourcc_t		ckID;	/// Chunk type identifier
		uint32_t		ckSize;		/// Chunk size
	};

	/// the riff chunk declaration
	struct chunk_t : chunk_header_t
	{
		uint8_t			ckData[];	/// Chunk data follows
	};
#pragma pack(pop)

	/**
	 * class Chunk is the base building component of the Wav file
	 */
	class Chunk
	{
	public:
		/** Create a new chunk with predefined buffer
		 *
		 * This is generic object made for easy creation of special instances
		 *
		 * arguments:
		 * parent - the chunk that contains this chunk
		 * header_size - the size of header of the new chunk
		 * ckID - the chunk ID, if the ckID is 0 while reading from file, then no ID check will be performed
		 */
		Chunk( Chunk *parent, uint32_t header_size = sizeof(chunk_t), fourcc_t ckID = 0 );

		/** Create a new root chunk based on file data
		 *
		 * arguments:
		 * fname - filename to open
		 * header_size - the size of header of the root chunk
		 * ckID - the chunk ID, for verification of file
		 */
		Chunk( std::string fname, uint32_t header_size, fourcc_t ckID );

		/** Return true if chunk is valid
		 *
		 */
		bool valid(){ chunk_t *hdr = (chunk_t*)_header->_field; return (hdr->ckID.asU32 != 0); };


		/** Get size of the chunk hierarchy
		 *
		 * return:
		 * total size of the chunk or 0 if empty
		 */
		virtual uint32_t getSize();

		/** Get size of data part excluding the header
		 *
		 * return:
		 * total size of the chunk data or 0 if empty
		 */
		virtual uint32_t getDataSize();

		/** Fill the buffer with chunk contents
		 *
		 * arguments:
		 * buf - the pointer to pointer of memory field to fill. The memory must be large enough for the process
		 * maxlen - reference to the value of the buffer remaining amount of bytes
		 *
		 * returns:
		 * buf value will be increased
		 * maxlen value will be decreased
		 * returns actual amount of bytes written, 0 on error
		 */
		virtual uint32_t fillBuffer( uint8_t **buf, uint32_t &maxlen );

		/** Write the contents into the file.
		 * Note: the chunks with null pointers are ignored.
		 *
		 * \param fp - the file pointer for fwrite
		 * \return number of bytes written.
		 * increments the fp internals (with fwrite);
		 */
		virtual uint32_t writeFile( FILE *fp );

		/** Get the pointer to "allocated" data field
		 *
		 * returns:
		 *  0 if no allocated data field
		 */
		virtual std::shared_ptr<MemBuffer> getData( uint32_t i=0 );

		/** add piece of externally managed data into list of data
		 *
		 * parameters:
		 * origin - pointer to the beginning of the buffer
		 * size - buffer length in bytes
		 */
		void addPiece(
				uint8_t *origin,		/// the origin of data
				uint32_t size );		/// the size of data

		/** add piece of internally managed data into list of data
		 *
		 * parameters:
		 * size - buffer length in bytes
		 *
		 * returns:
		 * pointer to the allocated memory
		 */
		std::shared_ptr<MemBuffer> addPiece( uint32_t size );		/// the size of data

		/** add piece of premade buffer
		 *
		 * parameters:
		 * buf - the buffer
		 */
		void addPiece( std::shared_ptr<MemBuffer> buf );

		/** Get number of contaned chunks
		 *
		 */
		uint32_t getContained() { return _contents.size(); };

		/** Add a chunk as child to this chunk
		 *
		 * arguments:
		 * child - the chunk to add as child
		 */
		void setChild( Chunk *child );

		/** Seek file to the end of chunk
		 *
		 */
		void seekFileEndOfChunk();

		/** seek file to start of data of chunk
		 *
		 */
		void seekFileStartOfData( int64_t dataseek=0 );

		/** seek file to start of chunk
		 *
		 */
		void seekFileStartOfChunk();

		/** Check if the current file location is in range
		 *
		 */
		bool inFileRange();

	protected:
		std::shared_ptr<MemBuffer> _header;
		std::vector< std::shared_ptr<MemBuffer> > _data;
		std::vector<Chunk*> _contents; // list of child chunks managed elsewhere
		uint32_t _min_size; /// minimum size of the chunk, consider it empty if less or this
		// when reading wav file from disk:
		std::shared_ptr<FileBuffer> _filebuf; // the file containing the data
		int64_t _filepos; // position in file where this chunk starts if read from _filebuf
	};

	/**
	 * LeafChunk base class
	 *
	 * LeafChunk is Chunk that does not contain other chunks.
	 * It does have data field of some other type than Chunk.
	 *
	 * This class is used to easily implement leaf chunks
	 */
	class LeafChunk : public Chunk
	{
	public:
		LeafChunk( Chunk *parent, uint32_t header_size, uint32_t data_size, fourcc_t ckID );
	};

#pragma pack(push, 1)
	/// the wave format declaration
	struct riff_chunk_t
	{
		chunk_header_t	riff; 			/// the riff part
		fourcc_t		formType;	/// the format identifier
		uint8_t			data[];
	};
#pragma pack(pop)

	/**
	 * RIFF chunk
	 *
	 * note: usually the Riff chunk is not contained by another chunk, although it is possible
	 */
	class RiffChunk : public Chunk
	{
	public:
		RiffChunk( fourcc_t formType );
		RiffChunk( std::string fname, fourcc_t formType );
	};

#pragma pack(push, 1)
	/// the wave format chunk
	struct wave_format_t
	{
		chunk_header_t	fmt;			/// the chunk part
		uint16_t		wFormatTag;		/// format category
		uint16_t		wChannels;		/// number of channels
		uint32_t		dwSamplesPerSec;/// sampling rate
		uint32_t		dwAvgBytesPerSec;/// for buffer estimateion
		uint16_t		wBlockAlign;	/// Data block size
	};
#pragma pack(pop)

	/**
	 * basic wave format chunk, used for creation concrete chunk
	 */
	class WaveChunk : public Chunk
	{
	public:
		WaveChunk( Chunk *parent, uint32_t header_size,
				uint16_t channels, 			/// number of channels
				uint32_t samples_per_sec,	/// sample rate
				uint16_t bytes_per_value	/// how many bytes per value is needed
		);
		WaveChunk( Chunk *parent );
	};

#pragma pack(push, 1)
	/// the Pulse Code Modulation (PCM) format; wFormatTag = 1
	struct pcm_format_t
	{
		wave_format_t	waveFmt;
		uint16_t		wBitsPerSample;	// Sample size
	};
#pragma pack(pop)

	/**
	 * Pulse Code Modulation chunk
	 */
	class PcmChunk : public WaveChunk
	{
	public:
		PcmChunk( Chunk *parent,
				uint16_t channels,
				uint32_t samples_per_sec,
				uint16_t bits_per_sample
		);
		PcmChunk( Chunk *parent );
		pcm_format_t *getPcmFormat() { return (pcm_format_t *)_header->_field; };
	};

#pragma pack(push, 1)
	/// Wave data chunk
	struct wave_data_chunk_t
	{
		/// The chunk part
		/// The samples follow the header immediately.
		chunk_header_t	data;
	};
#pragma pack(pop)


	/**
	 * PCM data chunk with one or multiple fraction
	 */
	class PcmDataChunk : public Chunk
	{
	public:
		/// chunk owns the buffer of waveform
		PcmDataChunk( Chunk *parent): Chunk( parent, sizeof(wave_data_chunk_t), "data" ), _row_length(0), _ratefactor(0), _nchannels(0)
		{
			setSampleFactor();
		}

		/// set samplerate reduction factor for saving time
		/// important assumption is that the data is signed 16bits for value
		void setSampleFactor( uint32_t factor = 1, uint32_t nchannels = 0){
			_ratefactor = factor > 0 ? ( factor <= 100000 ? factor : 100000 ) : 1 ;
			_nchannels = nchannels;
		}

		void setSampleWidth(unsigned int widthInBits);

		/** Write the file directly into file
		 *
		 * arguments:
		 * fp - the file pointer for fwrite
		 *
		 * returns:
		 * number of bytes written
		 * increments the fp internals (with fwrite);
		 */
		virtual uint32_t writeFile( FILE *fp ) override;

		/** Get size of data part excluding the header
		 *
		 * return:
		 * total size of the chunk data or 0 if empty
		 */
		virtual uint32_t getDataSize() override;

		/** sample iterator
		 *
		 * This is efficient way of traversing trhough the junked wav file data
		 */
		class SampleIterator{
		public:
			enum{ FIX= 16 }; //fixed point for fraction
			/**
			 * Important assumption: each buffer in data divides exactly with len
			 *
			 * chunk - the chunk where to iterate
			 * len - sample len in bytes
			 * index - starting index of the sample
			 * fraction - starting fraction of the sample index
			 */
			SampleIterator( PcmDataChunk *chunk, uint32_t len );
			/** get single sample from the data
			 * return:
			 * pointer to the sample of all channels, 0 if the sample does not exist
			 */
			virtual MemBufferSptr getSample( uint32_t count = 1 ) = 0;
			/** set iterator position
			 * return:
			 * pointer to this iterator
			 */
			virtual SampleIterator *setPos( uint32_t index, uint32_t fraction = 0 ) = 0;
			virtual SampleIterator *nextPos( uint32_t index=1, uint32_t fraction = 0 ) = 0;
			/** get single sample from the data and increase the iterator afterwards
			 * return:
			 * pointer to the sample of all channels, 0 if the sample does not exist
			 */
			virtual MemBufferSptr getSampleInc( uint32_t count = 1, uint32_t index=1, uint32_t fraction = 0 ) = 0;
			/** */
			virtual ~SampleIterator(){};
		protected:
			/// Length of the sample
			uint32_t	_samplelen;
			/// Chunk that contains the data
			PcmDataChunk *_chunk;
			/// Accumulated fraction, unity is (1<<16)
			uint32_t	_fraction;
		};

		friend class SampleIterator;
		friend class SampleIteratorFile;
		friend class SampleIteratorMemory;

		/** Get sample iterator
		 *
		 * len - sample size in bytes
		 * index - the sample index
		 * fraction - fractional index of sample
		 */
		std::shared_ptr<SampleIterator> getSampleIterator( uint32_t len, uint32_t index = 0, uint32_t fraction = 0 );

	protected:
		/// Length of the sample
		uint32_t	_row_length;
		/// the factor of reducing the PCM samplerate
		uint32_t	_ratefactor;
		/// number of channels in data
		uint32_t	_nchannels;
	};


#pragma pack(push, 1)
	/// cue-Point
	struct cue_point_t
	{
		fourcc_t		dwName;			/// unique name of cue point
		uint32_t		dwPosition;		/// sample position of cue point
		fourcc_t		fccChunk;		/// name of chunk containing the cue point
		uint32_t		dwChunkStart;	/// start position of the chunk containing the cue point
		uint32_t		dwBlockStart;	/// position of the start of the block containing the position
		uint32_t		dwSampleOffset;	/// sample offset of the cue point from start of the block
	};
#pragma pack(pop)

#pragma pack(push, 1)
	/// cue-Points chunk
	struct cue_chunk_t
	{
		chunk_header_t	cue;		/// the chunk part
		uint32_t		dwCuePoints;/// number of cue points
		cue_point_t		points[];	/// the array of cue points
	};
#pragma pack(pop)

	class CueChunk : public Chunk
	{
	public:
		CueChunk( Chunk *parent );

		void setPoint( fourcc_t name,
				fourcc_t chunk_name,
				uint32_t chunk_start,
				uint32_t block_start,
				uint32_t sample_offset
		);

		/// set point of single data chunk wav file
		void setWavPoint( const char* name,
				const char* chunk_name,
				uint32_t sample_offset
		);
	};

#pragma pack(push, 1)
	/// associated data list
	struct assoc_data_t
	{
		chunk_header_t	assoc;		/// the chunk part
		fourcc_t		fccAdtl;	/// the 'adtl'
		uint8_t			data[];		/// the list items
	};
#pragma pack(pop)

	/** Associated data list
	 *
	 * It is used to embed different kind of chunks into the wav file header
	 */
	class AssocListChunk : public Chunk
	{
	public:
		AssocListChunk( Chunk *parent );
	};


#pragma pack(push, 1)
	/// labl or note chunk
	struct label_chunk_t
	{
		chunk_header_t	labl;		/// the chunk part
		fourcc_t		fccName;	/// the cue point name
		char			str[];		/// the string 0 terminated
	};
#pragma pack(pop)

	/** Label
	 *
	 * Used for describing cue points in wav file header
	 */
	class LabelChunk : public LeafChunk
	{
	public:
		LabelChunk( Chunk *parent, fourcc_t name, const char *label );
		/// constructor for initiating chunk when describing file on disk
		LabelChunk( Chunk *parent ) : LeafChunk( parent, sizeof(label_chunk_t), 0, "labl" ){};
		label_chunk_t *getLabelChunkHeader(){ return (label_chunk_t*)_header->_field; };
	};

#pragma pack(push, 1)
	/// file chunk
	struct file_chunk_t
	{
		chunk_header_t	file;		/// the chunk part
		fourcc_t		name;		/// the name of the file
		fourcc_t		media;		/// the media type
		char			str[];		/// the string 0 terminated
	};
#pragma pack(pop)

	/** File
	 *
	 * Used for embedding any kind of files into the Wav file
	 */
	class FileChunk : public LeafChunk
	{
	public:
		FileChunk( Chunk *parent, fourcc_t name, fourcc_t media, const void *file, uint32_t file_size );
		/// constructor for initiating chunk when describing file on disk
		FileChunk( Chunk *parent ) : LeafChunk( parent, sizeof(file_chunk_t), 0, "file" ){};
		file_chunk_t *getFileChunkHeader(){ return (file_chunk_t*)_header->_field; };
	};
};

} // namespace smart

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
