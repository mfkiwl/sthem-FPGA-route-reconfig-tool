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

#ifndef JTAG_H
#define JTAG_H

#include "lynsyn_main.h"
#include "zynq.h"
#include "arm.h"
#include "../common/usbprotocol.h"

enum {
  ARMV7A,
  ARMV8A
};

enum {
  CORTEX_A9,
  CORTEX_A53,
  CORTEX_A57,
  DENVER_2
};

struct Core {
  unsigned type;
  unsigned core;
  unsigned ap;
  uint32_t baddr;
  bool enabled;
};

///////////////////////////////////////////////////////////////////////////////
// internal functions

uint64_t calcOffset(uint64_t dbgpcsr);

uint32_t coreReadReg(struct Core *core, uint16_t reg);
uint64_t coreReadPcsr(struct Core *core);

int getNumDevices(void);
void getIdCodes(uint32_t *idcodes);
void gotoResetThenIdle(void);

uint32_t dpLowAccess(uint8_t RnW, uint16_t addr, uint32_t value);

void writeIr(uint32_t idcode, uint32_t ir);
void readWriteDr(uint32_t idcode, uint8_t *din, uint8_t *dout, int size);

///////////////////////////////////////////////////////////////////////////////
// public functions

void jtagInit(void);

bool jtagInitCores(struct JtagDevice *devices); // call this at least every time a new board has been plugged in

void coreReadPcsrInit(void);
bool coreReadPcsrFast(uint64_t *pcs, bool *halted);

void coresResume(void);
void coreSetBp(unsigned core, unsigned bpNum, uint64_t addr);
void coreClearBp(unsigned core, unsigned bpNum);

void setBp(unsigned bpNum, uint64_t addr);
void clearBp(unsigned bpNum);

uint8_t coreReadStatus(unsigned core);
uint64_t readPc(unsigned core);

bool coreHalted(unsigned core);

uint32_t extractWord(unsigned *pos, uint8_t *buf);
uint8_t extractAck(unsigned *pos, uint8_t *buf);

///////////////////////////////////////////////////////////////////////////////

extern int numDevices;
extern struct JtagDevice devices[];
extern uint32_t dpIdcode;
extern unsigned numCores;
extern unsigned numEnabledCores;
extern struct Core cores[];

#endif
