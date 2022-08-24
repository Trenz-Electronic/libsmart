/// \file  AxiDataCaptureDevice.h
/// \brief Interface of the class AxiDataCaptureDevice.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH
#pragma once

#include <memory>	// std::shared_ptr
#include <functional>	// std::function

#include "../MappedFile.h"
#include "../UioDevice.h"

namespace smart {
namespace hw {

/// \brief Interface to the AXI Stream Capture IP core, which is exposed as an UIO device.
///
/// The required device tree properties of the UIO device node are listed in the following table:
/// <table>
///   <tr>
///     <th>Property name</th>
///     <th>Description</th>
///   </tr>
///   <tr>
///     <td>channels</td>
///     <td>Number of channels</td>
///   </tr>
///   <tr>
///     <td>cdata-width</td>
///     <td>Data width for one channel, in bits</td>
///   </tr>
///   <tr>
///     <td>sample-rate</td>
///     <td>Number of samples captured per second, for one channel</td>
///   </tr>
/// </table>
///
/// The UIO device must expose two memory maps: number 0 for accessing the IP core register, number 1 for access to the DMA buffer.
/// Example output of the command <em>lsuio</em>:
/// @code
/// root@plnx_arm:~# lsuio
/// uio0: name=AXI-Data-Capture, version=0.0.3, events=0
///         map[0]: addr=0x43C10000, size=65536
///         map[1]: addr=0x1F000000, size=4194304
/// @endcode
///
/// Example of device tree overrides:
/// @code
/// &AXI_Data_Capture_0 {
///         compatible = "trenz.biz,smartio-1.0";
///         trenz.biz,name = "AXI-Data-Capture";
///         trenz.biz,buffer-size = <0x400000>;
///         trenz.biz,sample-rate = <78125>;
/// };
/// @endcode
///
/// Example of C++ code for capturing complete DMA buffer:
/// @code
///	FILE*					fout = fopen("capture.bin", "wb");
///	AxiDataCaptureDevice	dev(AxiDataCapture::DEFAULT_UIO_NAME);
///	const unsigned int		capture_time_ms = dev.startCapture(dev.buffer->size(), 0);
///
///	msleep(capture_time_ms + 10u);
///	fwrite(dev.buffer->data(), 1, dev.buffer->size(), file);
///	fclose(fout);
/// @endcode
///
class AxiDataCapture {
private:
	/// UIO device.
	std::shared_ptr<smart::UioDevice>	m_device;

	/// Registers.
	smart::MappedFile*					m_registers;

	/// Data buffer.
	smart::MappedFile*					m_buffer_file;

	/// Buffer for the DMA operation.
	void*								m_buffer;

	/// Size of the buffer.
	const unsigned int					m_buffer_size;

	/// Start address of the buffer.
	const uint32_t						m_physical_start_addr;

	/// Offset of the tail.
	uint32_t							m_offset_tail;

	/// Start time of the capture, in ADC units.
	std::uint64_t						m_start_time_adc;

	/// Last transfer count before current capture.
	std::uint32_t						m_last_transfer_count;
public:
	static constexpr unsigned int CAPTURE_STREAMING = 0;

	/// Number of channels in the data capture.
	const unsigned int		nchannels;

	/// Number of bits in one sample (of one channel).
	const unsigned int		sample_width;

	/// Sample rate, samples per second.
	unsigned int			sample_rate;
public:

	/// Default name for the data capture device, "AXI-Data-Capture".
	static const char*		DEFAULT_UIO_NAME;

	/// This constructor initializes all the fields.
	/// \param pDevice	UIO device to be used as the capture device.
	AxiDataCapture(std::shared_ptr<smart::UioDevice>	pDevice);

	/// constructor.
	/// \param uio_name Name of the data capture UIO device. The constant #DEFAULT_UIO_NAME provides a name that should be used by default.
	AxiDataCapture(const char* uio_name);

	/// Destructor.
	~AxiDataCapture();

	/// Start data capture.
	/// \param size  Capture size, in bytes. The mode is set to streaming when 0.
	/// \return Capture time, in microseconds.
	unsigned int startCapture(const unsigned int size);

	/// Is capture in progress?
	/// \return true when a capture is in progress, false otherwise.
	bool isCaptureInProgress();

	/// Streaming mode only: Fetch the given amount of bytes, if possible.
	/// \param packetBuffer	Buffer to be used when the packet is split. Has to be at least the size of the packet.
	/// \param size			Size of the packet to be fetched.
	/// \param dmaOffset	Offset in the DMA buffer.
	/// \returns Pointer to the packet, null when not enough data available.
	void* fetchPacket(void* packetBuffer, const size_t packet_size);

	/// Streaming mode only: Fetch the given amount of bytes, if possible.
	/// \returns Pointer to the packet, null when not enough data available.
	template <typename TP>
	volatile TP* fetchPacket(TP* packetBuffer) {
		return reinterpret_cast<volatile TP*>(fetchPacket(reinterpret_cast<void*>(packetBuffer), sizeof(TP)));
	}

	void stopCapture();

	void clearBuffer();
};

} // namespace hw
} // namespace smart
