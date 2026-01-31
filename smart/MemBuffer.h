/*
 * MemBuffer.h
 *
 *  Created on: May 27, 2016
 *      Author: peeter
 */

#pragma once

#include <memory>
#include <stdint.h>

namespace smart {

class MemBuffer;
typedef std::shared_ptr<MemBuffer> MemBufferSptr;

/// Memory buffer handling
class MemBuffer
{
public:
	/// only shared pointer version is allowed
	/// buffer with externally managed memory
	static MemBufferSptr make_shared( void *data, size_t data_size  ){
		return MemBufferSptr( new MemBuffer(data, data_size) );
	};
	/// buffer with internally managed memory
	static MemBufferSptr make_shared( size_t data_size  ){
		return MemBufferSptr( new MemBuffer(data_size) );
	};
	/// sub-buffer with parent buffer memory management scheme
	static MemBufferSptr make_shared( MemBufferSptr parent, size_t offset, size_t data_size ){
		return MemBufferSptr( new MemBuffer(parent, offset, data_size) );
	};
	/// sub-buffer with parent buffer memory management scheme
	static MemBufferSptr make_shared( MemBufferSptr parent, void *data, size_t data_size ){
		return MemBufferSptr( new MemBuffer(parent, (uint8_t*)data - parent->_u8field, data_size) );
	};
	/// get parent buffer
	MemBufferSptr getParent(){ return _parent; };
public:
	union{
		void *_field;
		uint8_t *_u8field;
	};
	size_t _size;
protected:
	/// initialise buffer with externally managed memory
	MemBuffer( void *data, size_t data_size ){ _field = data; _size = data_size; _delete = false; _parent = nullptr; };
	/// initialise buffer with internally managed memory
	MemBuffer( size_t data_size ){ _field = malloc(data_size); _size = data_size; _delete = true; _parent = nullptr; };
	/// initialise buffer as sub-buffer
	MemBuffer( MemBufferSptr parent, size_t offset, size_t data_size ){
		_parent = parent; _delete = false;
		if( offset >= parent->_size ){ offset = 0; data_size = 0; }
		else if( (offset+data_size) > parent->_size ){ data_size = parent->_size - offset; }
		_field = parent->_u8field + offset; _size = data_size;
	}
	/// it is not allowed to assign one buffer to another, use shared_ptr !
	MemBuffer( MemBuffer &origin ){ _size=0;_field=0;_delete=false; _parent = nullptr; throw "Illegal operation with the buffer"; };
public:
	/// correctly free the memory if internally managed buffer
	virtual ~MemBuffer(){ if(_delete) free(_field); };
protected:
	bool _delete; // true if the buffer needs to be deleted on destruction
	MemBufferSptr _parent; // if the buffer is sub-part of another MemBuffer
};

} // namespace smart
