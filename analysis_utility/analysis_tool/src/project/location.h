/******************************************************************************
 *
 *  This file is part of the TULIPP Analysis Utility
 *
 *  Copyright 2018 Asbjørn Djupdal, NTNU, TULIPP EU Project
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef LOCATION_H
#define LOCATION_H

#include "pmu.h"

class Location {

private:
  void init(QString mid, QString fid, QString bid, BasicBlock *b) {
    moduleId = mid;
    funcId = fid;
    bbId = bid;
    bb = b;

    runtime = 0;
    runtimeFrame = 0;
    runtimeFrameAvg = 0;
    for(int i = 0; i < LYNSYN_SENSORS; i++) {
      energy[i] = 0;
      energyFrame[i] = 0;
      energyFrameAvg[i] = 0;
    }

    loopCount = 0;
  }

public:
  static int idCounter;
  int id;

  QString moduleId;
  QString funcId;
  QString bbId;
  BasicBlock *bb;

  std::map<int,int> callers;

  double runtime;
  double energy[LYNSYN_SENSORS];

  double runtimeFrame;
  double energyFrame[LYNSYN_SENSORS];

  double runtimeFrameAvg;
  double energyFrameAvg[LYNSYN_SENSORS];

  uint64_t loopCount;

  bool inDb;

  Location(int id, QString mid, QString fid, QString bid, BasicBlock *b) {
    this->id = id;
    inDb = true;
    init(mid, fid, bid, b);
  }

  Location(QString mid, QString fid, QString bid, BasicBlock *b) {
    id = idCounter++;
    inDb = false;
    init(mid, fid, bid, b);
  }

  void addCaller(int caller, int count) {
    if(callers.find(caller) != callers.end()) {
      callers[caller] = callers[caller] + count;
    } else {
      callers[caller] = count;
    }
  }

  void updateRuntime(double addedRuntime) {
    runtime += addedRuntime;
    runtimeFrame += addedRuntime;
  }

  void updateEnergy(unsigned sensor, double addedEnergy) {
    energy[sensor] += addedEnergy;
    energyFrame[sensor] += addedEnergy;
  }

  void clearFrameData() {
    runtimeFrame = 0;
    for(int i = 0; i < LYNSYN_SENSORS; i++) {
      energyFrame[i] = 0;
    }
  }

  void addToAvg(unsigned totalFrames) {
    runtimeFrameAvg += runtimeFrame / totalFrames;
    for(int i = 0; i < LYNSYN_SENSORS; i++) {
      energyFrameAvg[i] += energyFrame[i] / totalFrames;
    }
  }
};

#endif
