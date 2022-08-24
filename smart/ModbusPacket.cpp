#include <string>	// std::string
#include <stdexcept>	// std::runtime_error

#include <string.h>	// strchr.h

#include "ModbusPacket.h"
#include "string.h"

namespace smart {

static const char*  hexchars        = "0123456789ABCDEF";

/*****************************************************************************/
/** MSN is s[index] LSN is s[index+1], .
Throws exceptions on errors.
*/
static unsigned int
byte_of_modbus(
    const std::string&  s,
    const unsigned int  index)
{
    static const char*  hexchars = "0123456789ABCDEF";
    const unsigned int  msb = strchr(hexchars, s[index  ]) - hexchars;
    const unsigned int  lsb = strchr(hexchars, s[index+1]) - hexchars;

    if (lsb > 0x0F) {
        throw std::runtime_error(ssprintf("MODBUS: Not a hex char: %c", s[index]));
    }
    if (msb > 0x0F) {
        throw std::runtime_error(ssprintf("MODBUS: Not a hex char: %c", s[index+1]));
    }

    return lsb + (msb << 4);
}

/*****************************************************************************/
/** Modbus ASCII protocol checksum.
\param[in]  line    Line received (or to be sent)
\param[in]  n       Number of chars to take into account (including first semicolon).
\return LRC checksum.
*/
static unsigned char
lrc_of_line(
    const std::string&  line,
    const unsigned int  n)
{
    const unsigned int  npairs = (n-1) / 2;
    unsigned char       lrc = 0;
    for (unsigned int i=0; i<npairs; ++i) {
        const unsigned char b = byte_of_modbus(line, 1 + 2*i);
        lrc += b;
    }
    lrc = 256 - lrc;
    return lrc;
}

/*****************************************************************************/
/** Verify modbus LRC checksum for the given line. Note that line endings are not stripped!

Throws exceptions on errors.
\param[in]  line (example: ":0103FE000041BD").
*/
static void
verify_checksum( const std::string&   line)
{
    bool    ok = false;
    if (line.size()<7) {
        throw std::runtime_error(ssprintf("MODBUS: line length %d is too small (should be at least 7)!", line.c_str()));
    }

    if (line[0] != ':') {
        throw std::runtime_error(ssprintf("MODBUS: line start '%c' is not colon ':'!", line[0]));
    }

    if ((line.size()&0x01) != 0x01) {
        throw std::runtime_error(ssprintf("MODBUS: line length %d is not even!", line.length()));
    }
    const unsigned char check_rx = byte_of_modbus(line, line.size()-2);
    const unsigned char check_lrc = lrc_of_line(line, line.size()-2);
    if (check_rx != check_lrc) {
        throw std::runtime_error(ssprintf("MODBUS: invalid checksum 0x%02X, should be 0x%02X",
            (unsigned int)check_rx, (unsigned int)check_lrc));
    }

    // must be ok by now.
}

/*****************************************************************************/
static void
append_byte(
    std::string&        buffer,
    const unsigned char b)
{
    buffer += hexchars[b >> 4];
    buffer += hexchars[b & 0x0F];
}

/*****************************************************************************/
static void
append_word(
    std::string&            buffer,
    const unsigned short    w)
{
    append_byte(buffer, w >> 8);
    append_byte(buffer, w & 0xFF);
}

/*****************************************************************************/
bool
ModbusPacket::IsLineValid(
    const std::string&  line)
{
    // 1. Big enough? (':' + 2 bytes address + 2 bytes function + 2 bytes lrc)
    // 2. Colon at the beginning?
    // 3. Even number of bytes.
    const bool ok1 =
        line.size()>=7 &&
        line[0] == ':' &&
        (line.size() & 0x01)==0x01;
    if (!ok1) {
        return false;
    }

    // All characters are hexadecimal.
    for (unsigned int i=1; i<line.size(); ++i) {
        if (strchr(hexchars, line[i])==0) {
            return false;
        }
    }

    // Is checksum OK?
    const bool  checksum_ok =
        lrc_of_line(line, line.size()-2) == byte_of_modbus(line, line.size()-2);
    if (!checksum_ok) {
        return false;
    }

    return true;
}

/*****************************************************************************/
void
ModbusPacket::ImportAscii(
    const std::string&  line)
{
    // Verify checksum and all other errors, too.
    verify_checksum(line);

    //
    Address = byte_of_modbus(line, 1);
    Function = byte_of_modbus(line, 3);
    const unsigned int  npairs = (line.size()- 7) / 2;
    Data.resize(npairs);
    for (unsigned int i=0; i<npairs; ++i) {
        Data[i] = byte_of_modbus(line, 5 + 2*i);
    }
}

/*****************************************************************************/
void
ModbusPacket::ImportResponseHoldingRegisters(
    const unsigned int                  address,
    const std::vector<unsigned char>&   registers
)
{
    Address = address;
    Function = FUNCTION_READ_HOLDING_REGISTERS;
    Data.resize(registers.size() + 1);
    Data[0] = registers.size();
    for (unsigned int i=0; i<registers.size(); ++i) {
        Data[i + 1] = registers[i];
    }
}

void ModbusPacket::AddDataUInt16(const uint16_t& t)
{
	Data.push_back(t >> 8);
	Data.push_back(t);
}

/*****************************************************************************/
std::string
ModbusPacket::ToString() const
{
    std::string r;
    r += ':';
    append_byte(r, Address);
    append_byte(r, Function);
    for (unsigned int i=0; i<Data.size(); ++i) {
        append_byte(r, Data[i]);
    }
    const unsigned int lrc = lrc_of_line(r, r.size());
    append_byte(r, lrc);
    return r;
}

/*****************************************************************************/
void
ModbusPacket::ToReadHoldingRegisters(
        unsigned int&   address,
        unsigned int&   word_count
        )
{
    if (Function == FUNCTION_READ_HOLDING_REGISTERS) {
        if (Data.size() == 4) {
            address     = (((unsigned int)Data[0]) << 8) + Data[1];
            word_count  = (((unsigned int)Data[2]) << 8) + Data[3];
        } else {
            throw std::runtime_error(ssprintf("ModbusPacket::ToReadHoldingRegisters: Expected data length 4, got %d instead!",
                Data.size()));
        }
    } else {
        throw std::runtime_error(ssprintf("ModbusPacket::ToReadHoldingRegisters: Expected function 0x03, got 0x%02x instead!",
                Function));
    }
}

/*****************************************************************************/
void
ModbusPacket::Clear()
{
    Function = 0;
    Address = 0;
    Data.resize(0);
}

} // namespace utils


