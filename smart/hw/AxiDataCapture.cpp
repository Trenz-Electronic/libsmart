/// \file  AxiDataCapture.cpp
/// \brief Implementation of the class AxiDataCapture.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH
#include <inttypes.h>		// PRIx64, etc.
#include <string.h>			// memcpys

#include "AxiDataCapture.h"


#include "../time.h"
#include "../File.h"



namespace smart {
namespace hw {

enum class Register : unsigned int {
	CONTROL=0,
	START_ADDRESS=1,
	BLOCKS_PER_TRANSFER=2,
	BLOCK_SIZE=3,
	BLOCKS_PER_RING=4,
	BLOCKS_TRANSFERRED=5,
	CURRENT_BLOCK=6,
	CURRENT_ADDRESS=7,
	BURST_ERROR_COUNT=8,
	BURST_SUCCESS_COUNT=9,
};

/// Block size, in bytes.
/// For the Zynq 32-bit the maximum value is 128.
static constexpr unsigned int BLOCK_SIZE = 128u;

static void write_reg(MappedFile* regs, const Register index, const uint32_t v)
{
	regs->write32(static_cast<unsigned int>(index), v);
}

static unsigned int read_reg(MappedFile* regs, const Register index)
{
	const unsigned int r = regs->read32(static_cast<unsigned int>(index));
	return r;
}

// --------------------------------------------------------------------------------------------------------------------
const char*		AxiDataCapture::DEFAULT_UIO_NAME = "AXI-Data-Capture";

/// trenz.biz,capture-channels [UInt32] (9)
static const std::string	DEVICETREE_CAPTURE_CHANNELS("capture-channels");

/// trenz.biz,cdata-width [UInt32] (16)
static const std::string	DEVICETREE_CDATA_WIDTH("cdata-width");

/// trenz.biz,channels [UInt32] (9)
static const std::string	DEVICETREE_CHANNELS("channels");

/// Sample rate of the data capture.
static const std::string	DEVICETREE_SAMPLE_RATE("sample-rate");


enum {
	/// 0=>1 triggers.
	BV_CONTROL_SOFTTRIGGER = 1 << 0,

	/// Tell the internal FIFO to hold the data instead of just ignoring it.
	/// This has to be set for the duration of the data transfer.
	BV_CONTROL_DATAHOLD = 1 << 1,
};

// --------------------------------------------------------------------------------------------------------------------
AxiDataCapture::AxiDataCapture(std::shared_ptr<smart::UioDevice>	pDevice)
: m_device(pDevice),
  m_registers(m_device->getRequiredMap(0)),
  m_buffer_file(m_device->getRequiredMap(1)),
  m_buffer(m_buffer_file->data()),
  m_buffer_size(m_buffer_file->size()),
  m_physical_start_addr(m_device->maps[1].addr),
  m_offset_tail(0u),
  m_start_time_adc(0),
  m_last_transfer_count(0),
  nchannels(m_device->getConfigurationUInt32(DEVICETREE_CHANNELS)),
  sample_width(m_device->getConfigurationUInt32(DEVICETREE_CDATA_WIDTH)),
  sample_rate(m_device->getConfigurationUInt32(DEVICETREE_SAMPLE_RATE))
{
	const unsigned int block_count = m_buffer_size / BLOCK_SIZE;
	write_reg(m_registers, Register::CONTROL, 0);
	write_reg(m_registers, Register::START_ADDRESS, m_device->maps[1].addr);
	write_reg(m_registers, Register::BLOCKS_PER_TRANSFER, block_count);
	write_reg(m_registers, Register::BLOCK_SIZE, BLOCK_SIZE);
	write_reg(m_registers, Register::BLOCKS_PER_RING, block_count); // the buffer in blocks
	m_last_transfer_count = read_reg(m_registers, Register::BLOCKS_TRANSFERRED);
}

// --------------------------------------------------------------------------------------------------------------------
AxiDataCapture::AxiDataCapture(const char* uio_name)
	: AxiDataCapture(std::make_shared<smart::UioDevice>(uio_name))
{
}

// --------------------------------------------------------------------------------------------------------------------
AxiDataCapture::~AxiDataCapture()
{
	if (m_registers) {
		write_reg(m_registers, Register::CONTROL, 0);
	}
}

// --------------------------------------------------------------------------------------------------------------------
unsigned int AxiDataCapture::startCapture(const unsigned int transfer_size)
{
	unsigned int capture_time_us;

	write_reg(m_registers, Register::CONTROL, 0); // Transfer has to be disabled for a moment, otherwise the trigger won't work.
	if (transfer_size == CAPTURE_STREAMING) {
		capture_time_us = 0;

		// Setup IP-core.
		write_reg(m_registers, Register::BLOCKS_PER_TRANSFER, 0);  // 0: Streaming mode !!!
	}
	else {
		const unsigned int	bytes_per_sample = (sample_width * nchannels) / 8u;
		const unsigned int	nsamples = transfer_size / bytes_per_sample;

		capture_time_us = (nsamples * static_cast<uint64_t>(1000U * 1000U)) / sample_rate;
		write_reg(m_registers, Register::CONTROL, BV_CONTROL_DATAHOLD);
		write_reg(m_registers, Register::BLOCKS_PER_TRANSFER, nsamples * bytes_per_sample);
		m_last_transfer_count = read_reg(m_registers, Register::BLOCKS_TRANSFERRED); // Record the transfer count so far.
	}
	write_reg(m_registers, Register::CONTROL, BV_CONTROL_SOFTTRIGGER | BV_CONTROL_DATAHOLD); // Start the trigger sequence.
	m_offset_tail = read_reg(m_registers, Register::CURRENT_ADDRESS) - m_physical_start_addr;

	return capture_time_us;
}

// --------------------------------------------------------------------------------------------------------------------
bool AxiDataCapture::isCaptureInProgress()
{
	const uint32_t	new_transfer_count = read_reg(m_registers, Register::BLOCKS_TRANSFERRED);
	if (new_transfer_count == m_last_transfer_count) {
		const uint32_t	control = read_reg(m_registers, Register::CONTROL);
		if ((control & BV_CONTROL_SOFTTRIGGER) != 0u) {
			return true;
		}
	}
	else {
		/// Completed.
		if (read_reg(m_registers, Register::BLOCKS_PER_TRANSFER)>0u) {
			write_reg(m_registers, Register::CONTROL, 0);
		}
	}
	return false;
}

void* AxiDataCapture::fetchPacket(void* packetBuffer, const size_t packet_size)
{
	const unsigned int	head_addr = read_reg(m_registers, Register::CURRENT_ADDRESS);
	const unsigned int	head = head_addr - m_physical_start_addr;
	const unsigned int	tail = m_offset_tail;
	const bool			split_read = head < tail;

	// How much is to be written this round?
	const unsigned int	total_available = split_read ? (head + m_buffer_size - tail) : (head - tail);
	if (total_available < packet_size) {
		return nullptr;
	}

	unsigned int	next_tail = tail + packet_size;
	// Easy case.
	if (next_tail <= m_buffer_size) {
		m_offset_tail = next_tail % m_buffer_size;
		uint8_t* packet = &reinterpret_cast<uint8_t*>(m_buffer)[tail];
		return packet;
	}

	uint8_t* dma_buffer = reinterpret_cast<uint8_t*>(m_buffer);
	uint8_t* packet_buffer = reinterpret_cast<uint8_t*>(packetBuffer);
	const unsigned int	size1 = m_buffer_size - tail;
	memcpy(&packet_buffer[0], &dma_buffer[tail], size1);
	next_tail = next_tail - m_buffer_size;
	memcpy(&packet_buffer[size1], &dma_buffer[0], next_tail);
	m_offset_tail = next_tail;
	return packet_buffer;
}

void AxiDataCapture::stopCapture()
{
	write_reg(m_registers, Register::CONTROL, 0); // Transfer has to be disabled for a moment, otherwise the trigger won't work.
	m_offset_tail = 0;
}

void AxiDataCapture::clearBuffer()
{
	memset(m_buffer, 0, m_buffer_size);
}


} // namespace hw
} // namespace smart
