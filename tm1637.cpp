//
// AVR TM1637 "Library" v1.01
// Enables control of TM1637 chip based modules, using direct port access.
//
// Copyright (c) 2015 IronCreek Software
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <avr/io.h>
#include <util/delay.h>
#include "tm1637.h"
#include "tm1637font.h"

#define TM_CLK_LOW()            (TM_OUT &= ~TM_BIT_CLK)
#define TM_CLK_HIGH()           (TM_OUT |= TM_BIT_CLK)
#define TM_DAT_LOW()            (TM_OUT &= ~TM_BIT_DAT)
#define TM_DAT_HIGH()           (TM_OUT |= TM_BIT_DAT)

#define TM_CLK_FLOAT()

// Instructions
#define TM_DATA_CMD             0x40
#define TM_DISP_CTRL            0x80
#define TM_ADDR_CMD             0xC0

// Data command set
#define TM_WRITE_DISP           0x00
#define TM_READ_KEYS            0x02
#define TM_FIXED_ADDR           0x04

// Display control command
#define TM_DISP_PWM_MASK        0x07 // First 3 bits are brightness (PWM controlled)
#define TM_DISP_ENABLE          0x08

#define DELAY_US                1

namespace
{
    void port_setup()
    {
        TM_DDR |= TM_BIT_CLK | TM_BIT_DAT;
        TM_OUT |= TM_BIT_CLK;
    }

    void start()
    {
        TM_CLK_HIGH();
        TM_DAT_HIGH();
        _delay_us(DELAY_US);

        TM_DAT_LOW();
        _delay_us(DELAY_US);
    }

    void stop()
    {
        TM_CLK_LOW();
        _delay_us(DELAY_US);

        TM_CLK_HIGH();
        TM_DAT_LOW();
        _delay_us(DELAY_US);

        TM_DAT_HIGH();
    }

    void send(uint8_t b)
    {
        // Clock data bits

        for (uint8_t i = 8; i; --i, b >>= 1)
        {
            TM_CLK_LOW();
            if (b & 1)
                TM_DAT_HIGH();
            else
                TM_DAT_LOW();
            _delay_us(DELAY_US);

            TM_CLK_HIGH();
            _delay_us(DELAY_US);
        }

        // Clock out ACK bit; not checking if it worked...

        TM_CLK_LOW();
        TM_DAT_LOW();
        _delay_us(DELAY_US);

        TM_CLK_HIGH();
        _delay_us(DELAY_US);
    }

    void send_cmd(const uint8_t cmd)
    {
        start();
        send(cmd);
        stop();
    }

    void send_data(const uint8_t addr, const uint8_t data)
    {
        send_cmd(TM_DATA_CMD | TM_FIXED_ADDR);

        start();
        send(TM_ADDR_CMD | addr);
        send(data);
        stop();

        _delay_us(DELAY_US);
    }

    uint8_t offset_digits(uint32_t num)
    {
        uint8_t digits = 0;
        while (num >= 10)
        {
            num /= 10;
            ++digits;
        }
        return digits;
    }
}

TM1637::TM1637()
    : _dotMask(0)
{
    port_setup();

    send_cmd(TM_DATA_CMD | TM_WRITE_DISP);
    send_cmd(TM_DISP_CTRL | TM_DISP_ENABLE | TM_DISP_PWM_MASK);

    clear();
}

void TM1637::clear()
{
    for (uint8_t a = 0; a != TM1637_DIGITS; ++a)
        send_data(a, 0x00);
}

void TM1637::setByte(const uint8_t position, const uint8_t b)
{
    send_data(position, b | (_dotMask & (1 << position) ? TM_DOT : 0));
}

void TM1637::setDigit(const uint8_t position, const uint8_t digit)
{
    setByte(position, pgm_read_byte(&TM_DIGITS[digit & 0xF]));
}

void TM1637::setNumber(uint32_t number, uint8_t offset, const uint8_t align)
{
    if (align == TM_LEFT)
        offset += offset_digits(number);

    while (number && offset != 0xFF)
    {
        setDigit(offset--, number % 10);
        number /= 10;
    }
}

void TM1637::setNumberPad(uint32_t number, uint8_t offset, uint8_t width, const uint8_t pad)
{
    while (number && width-- && offset != 0xFF)
    {
        setDigit(offset--, number % 10);
        number /= 10;
    }

    while (width -- && offset != 0xFF)
        setByte(offset--, pad);
}

void TM1637::setNumberHex(uint32_t number, uint8_t offset, uint8_t width, const uint8_t pad)
{
    while (number && width-- && offset != 0xFF)
    {
        setDigit(offset--, number & 0x0F);
        number >>= 4;
    }

    while (width -- && offset != 0xFF)
        setByte(offset--, pad);
}

void TM1637::setChar(const uint8_t position, const char ch)
{
    const uint8_t b = TM1637_map_char(ch);
    if (b || ch == ' ')
        setByte(position, b);

    else if (ch >= 'a' && ch <= 'z')
        setByte(position, pgm_read_byte(&TM_DIGITS[ch - 'a' + 10]));

    else if (ch >= 'A' && ch <= 'Z')
        setByte(position, pgm_read_byte(&TM_DIGITS[ch - 'A' + 10]));

    else if (ch >= '0' && ch <= '9')
        setByte(position, pgm_read_byte(&TM_DIGITS[ch - '0']));
}

void TM1637::setChars(const char* value, uint8_t offset)
{
    while (*value)
        setChar(offset++, *value++);
}

void TM1637::scrollChars(const char* value)
{
    uint32_t offset = 0;
    if (value == 0 || *value == 0)
        return;

    while(1)
    {
        for (uint8_t i = 0; i != TM1637_DIGITS; ++i)
        {
            const char *p = value + offset;
            setChar(i, p[i]);

            if (!p[i+1])
            {
                _delay_ms(250);
                return;
            }
        }

        ++offset;
        _delay_ms(250);
    }
}

void TM1637::setDots(const uint8_t mask)
{
    _dotMask = mask;
}

void TM1637::setBrightness(const uint8_t brightness)
{
    send_cmd(TM_DISP_CTRL | TM_DISP_ENABLE | (brightness & TM_DISP_PWM_MASK));
}

