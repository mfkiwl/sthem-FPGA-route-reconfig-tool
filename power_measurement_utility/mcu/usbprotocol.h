/******************************************************************************
 *
 *  This file is part of the Lynsyn PMU firmware.
 *
 *  Copyright 2018 Asbjørn Djupdal, NTNU, TULIPP EU Project
 *
 *  Lynsyn is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Lynsyn is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Lynsyn.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef USBPROTOCOL_H
#define USBPROTOCOL_H

#include <stdint.h>

#define USB_PROTOCOL_VERSION 1

#define HW_VERSION_2_0 0x20
#define HW_VERSION_2_1 0x21

///////////////////////////////////////////////////////////////////////////////

#define USB_CMD_INIT           'i'
#define USB_CMD_HW_INIT        'h'
#define USB_CMD_JTAG_INIT      'j'
#define USB_CMD_BREAKPOINT     'b'
#define USB_CMD_START_SAMPLING 's'
#define USB_CMD_CAL            'l'
#define USB_CMD_TEST           't'

#define BP_TYPE_START 0
#define BP_TYPE_STOP  1

#define TEST_USB      0
#define TEST_SPI      1
#define TEST_JTAG     2
#define TEST_OSC      3
#define TEST_LEDS_ON  4
#define TEST_LEDS_OFF 5
#define TEST_ADC      6

#define FPGA_INIT_OK          0
#define FPGA_CONFIGURE_FAILED 1
#define FPGA_SPI_FAILED       2

///////////////////////////////////////////////////////////////////////////////

#define MAX_PACKET_SIZE (sizeof(struct BreakpointRequestPacket))

struct __attribute__((__packed__)) RequestPacket {
  uint8_t cmd;
};

struct __attribute__((__packed__)) BreakpointRequestPacket {
  struct RequestPacket request;
  uint8_t core;
  uint8_t bpType;
  uint64_t addr;
};

struct __attribute__((__packed__)) HwInitRequestPacket {
  struct RequestPacket request;
  uint8_t hwVersion;
};

struct __attribute__((__packed__)) CalibrateRequestPacket {
  struct RequestPacket request;
  uint8_t channel;
  int32_t calVal;
  bool hw;
};

struct __attribute__((__packed__)) TestRequestPacket {
  struct RequestPacket request;
  uint8_t testNum;
};

///////////////////////////////////////////////////////////////////////////////

struct __attribute__((__packed__)) InitReplyPacket {
  uint8_t hwVersion;
  uint8_t swVersion;
  double calibration[7];
};

struct __attribute__((__packed__)) TestReplyPacket {
  uint32_t testStatus;
};

struct __attribute__((__packed__)) UsbTestReplyPacket {
  uint8_t buf[256];
};

struct __attribute__((__packed__)) AdcTestReplyPacket {
  int16_t current[7];
};

struct __attribute__((__packed__)) SampleReplyPacket {
  int64_t time;
  uint64_t pc[4];
  int16_t current[7];
};

///////////////////////////////////////////////////////////////////////////////

#endif
