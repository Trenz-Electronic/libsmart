#ifndef utils_modbus_h_
#define	utils_modbus_h_

#include <string>	// std::string
#include <vector>   // std::vector
#include <stdexcept>	// std::runtime_error

#include <stdint.h>	// uint16_t

namespace smart {

/// Encapsulate Modbus functions.
/// ModbusPacket	inp;
/// inp.ImportAscii(":0103FE000041BD");
/// printf("address: %d Function:%d\n", inp.Address, inp.Function);
///
/// ModbusPacket	outp;
/// outp.Address = inp.Address;
/// outp.Function = inp.Function;
/// outp.Data.push_back(11);
/// outp.Data.push_back(12);
/// ... usw.
/// std::string s = outp.ToString();
/// printf("response: %s\n", s.c_str());

class ModbusPacket {
public:
/// Modbus Functions.
typedef enum {
    FUNCTION_READ_COILS                 = 0x01,
    FUNCTION_READ_DISCRETE_INPUTS       = 0x02,
    FUNCTION_READ_HOLDING_REGISTERS     = 0x03,
    FUNCTION_READ_INPUT_REGISTERS       = 0x04,
    FUNCTION_WRITE_SINGLE_COIL          = 0x05,
    FUNCTION_WRITE_SINGLE_REGISTER      = 0x06,
    FUNCTION_READ_EXCEPTION_STATUS      = 0x07,
    FUNCTION_DIAGNOSTICS                = 0x08,
    FUNCTION_GET_COMM_EVENT_COUNTER     = 0x0B,
    FUNCTION_GET_COMM_EVENT_LOG         = 0x0C,
    FUNCTION_WRITE_MULTIPLE_COILS       = 0x0F,
    FUNCTION_WRITE_MULTIPLE_REGISTERS   = 0x10,
    FUNCTION_REPORT_SLAVE_ID            = 0x11,
    FUNCTION_READ_FILE_RECORD           = 0x14,
    FUNCTION_WRITE_FILE_RECORD          = 0x15,
    FUNCTION_MASK_WRITE_REGISTER        = 0x16,
    FUNCTION_READWRITE_MULTIPLE_REGISTERS   = 0x17,
    FUNCTION_READ_FIFO_QUEUE                = 0x18,
    FUNCTION_ENCAPSULATED_INTERFACE_REPORT  = 0x2B,
} FUNCTION;

    unsigned int                Address;
    unsigned int                Function;

    /// 8-bit raw data to be sent.
    std::vector<uint8_t>		Data;

    /** Is it valid Modbus ASCII line? */
    static bool
    IsLineValid(
        const std::string&  line);

    /** Import Modbus ASCII line. */
    void
    ImportAscii(
        const std::string&  line);

    /** Import data for the response to function \c FUNCTION_READ_HOLDING_REGISTERS.

    */
    void
    ImportResponseHoldingRegisters(
        const unsigned int                  address,
        const std::vector<unsigned char>&   registers
    );

    void AddDataUInt16(const uint16_t& t);

    /** String representation. */
    std::string
    ToString() const;

    /** Parse data for function \c FUNCTION_READ_HOLDING_REGISTERS.
    */
    void
    ToReadHoldingRegisters(
        unsigned int&   address,
        unsigned int&   word_count
        );

    /** Clear all members. */
    void
    Clear();
}; // class ModbusPacket

} // namespace utils

#endif /* utils_modbus_h_ */

