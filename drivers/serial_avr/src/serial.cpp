#include <serial.h>
#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <utl/algorithm.h>
#include <utl/execution.h>
#include <utl/iterator.h>
#include <utl/memory.h>
#include <algorithm>

static uint8_t tx_buffer[64];
static uint8_t rx_buffer[64];

using bufIterator = uint8_t*;

static bufIterator it_txRead = utl::begin(tx_buffer);
static bufIterator it_txWrite = utl::begin(tx_buffer);

static bufIterator it_rxRead = utl::begin(rx_buffer);
static bufIterator it_rxWrite = utl::begin(rx_buffer);

namespace utl {
template <class InputIt, class OutputIt> auto copy_p(InputIt first, InputIt last, OutputIt d_first) {
    while (first != last) {
        if constexpr (sizeof(*first) == 1)
            *d_first++ = pgm_read_byte(first++);
        else
            static_assert(sizeof(*first) == 1);
    }
    return d_first;
}
}  // namespace utl
void Serial::init() {
    // Set baud rate
    uint16_t UBRR0_value;
    int multiplier;
    if constexpr (baudrate < 57600) {
        multiplier = 8;
        UCSR0A = UCSR0A & ~(1 << U2X0);  // disable double baudrate bit
    } else {
        multiplier = 4;
        UCSR0A = UCSR0A | (1 << U2X0);  // enable double baudrate bit
    }
    UBRR0_value = (((F_CPU / (multiplier * baudrate)) - 1) / 2);

    UBRR0H = UBRR0_value >> 8;
    UBRR0L = UBRR0_value;

    // enable rx and tx
    UCSR0B = UCSR0B | 1 << RXEN0;
    UCSR0B = UCSR0B | 1 << TXEN0;

    // enable interrupt on complete reception of a byte
    UCSR0B = UCSR0B | 1 << RXCIE0;
}

auto Serial::tx(const char* first, const char* last, bool progmem) -> uint8_t {
    const uint8_t* uint8_first = reinterpret_cast<const uint8_t*>(first);
    const uint8_t* uint8_last = reinterpret_cast<const uint8_t*>(last);
    return tx(uint8_first, uint8_last, progmem);
}

auto Serial::tx(const uint8_t* first, const uint8_t* last, bool progmem) -> uint8_t {
    if (first == last) /*nothing to copy */
        return 0;

    uint8_t spaceNeeded = utl::distance(first, last);
    uint8_t spaceLeft = tx_spaceLeft();

    if (spaceNeeded > spaceLeft)
        return 0;

    bufIterator new_it_txWrite;
    if (progmem)
        new_it_txWrite = utl::copy_p(first, last, utl::CycleIt(tx_buffer, it_txWrite)).get();
    else
        new_it_txWrite =
            std::copy_if(first, last, utl::CycleIt(tx_buffer, it_txWrite), [](auto character) {
                return character != 0;
            }).get();

    /* update it_txWrite atomically */
    {
        utl::ClearInterrupts clear;
        it_txWrite = new_it_txWrite;
    }
    UCSR0B = UCSR0B | (1 << UDRIE0);  // start streaming Tx
    return spaceNeeded;
}

auto Serial::tx_spaceLeft() -> uint8_t {
    bufIterator temp_it_txRead;
    { /* because of a possible race condition, clear interrupts, and safe read tx iterator in
         a temporary */
        utl::ClearInterrupts clear;
        temp_it_txRead = it_txRead;
    }
    if ((0 == (UCSR0B & (1 << UDRIE0))) && temp_it_txRead == it_txWrite)
        return utl::size(tx_buffer);
    return utl::distance(utl::CycleIt(tx_buffer, it_txWrite), utl::CycleIt(tx_buffer, temp_it_txRead));
}

ISR(USART_UDRE_vect) {
    UDR0 = *it_txRead;
    it_txRead = (++utl::CycleIt(tx_buffer, it_txRead)).get();

    if (it_txRead == it_txWrite)
        UCSR0B = UCSR0B & ~(1 << UDRIE0);  // stop streaming Tx
}

ISR(USART_RX_vect) {
    *it_rxWrite = UDR0;
    Serial::tx(it_rxWrite, it_rxWrite + 1);
    it_rxWrite = (++utl::CycleIt(rx_buffer, it_rxWrite)).get();

    if (it_rxWrite == it_rxRead)
        UCSR0B = UCSR0B & ~(1 << RXCIE0);  // stop streaming Rx
}

extern "C" void _putchar(char character) { *utl::CycleIt(tx_buffer, it_txWrite)++ = character; }
