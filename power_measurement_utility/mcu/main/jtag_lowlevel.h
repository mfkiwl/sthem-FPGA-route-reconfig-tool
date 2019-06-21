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

#ifndef JTAG_LOWLEVEL_H
#define JTAG_LOWLEVEL_H

#include "../common/usbprotocol.h"

#include <stdbool.h>

void readWriteSeq(unsigned size, uint8_t *tdiData, uint8_t *tmsData, uint8_t *tdoData);
void storeSeq(uint16_t size, uint8_t *tdiData, uint8_t *tmsData);
void storeProg(unsigned size, uint8_t *read, uint16_t *initPos, uint16_t *loopPos, uint16_t *ackPos, uint16_t *endPos);
void executeSeq(void);
uint32_t *getWords();

bool jtagInitLowLevel(void);

#endif
