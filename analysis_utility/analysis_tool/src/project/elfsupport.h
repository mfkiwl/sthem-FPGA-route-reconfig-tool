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

#ifndef ELFSUPPORT_H
#define ELFSUPPORT_H

#include <QString>
#include <QStringList>
#include <QMap>

#include <map>
#include <sstream>

class Addr2Line {
public:
  QString filename;
  QString elfName;
  QString function;
  uint64_t lineNumber;

  Addr2Line() {
    filename = "";
    elfName = "";
    function = "";
    lineNumber = 0;
  }

  Addr2Line(QString filename, QString elfName, QString function, uint64_t lineNumber) {
    this->filename = filename;
    this->elfName = elfName;
    this->function = function;
    this->lineNumber = lineNumber;
  }
};

struct Offset {
  uint64_t offset;
  uint64_t size;

  Offset() {
    offset = size = 0;
  }

  Offset(uint64_t o, uint64_t s) {
    offset = o;
    size = s;
  }
};

class ElfSupport {

private:
  std::map<uint64_t, Addr2Line> addr2lineCache;

  QStringList elfFiles;
  QString symsFile;
  uint64_t prevPc;
  QMap<QString,Offset> elfOffsets;
  QMap<QString,bool> elfStatic;

  Addr2Line addr2line;

  void setPc(uint64_t pc);
  bool isStatic(QString elf);

public:
  ElfSupport() {
    prevPc = -1;
  }
  void addElf(QString elfFile) {
    if(elfFile.trimmed() != "") {
      elfFiles.push_back(elfFile);
    }
  }
  void addKallsyms(QString symsFile) {
    this->symsFile = symsFile;
  }
  void addElfOffsetsFromFile(QString offsetFile);

  // get debug info
  QString getFilename(uint64_t pc);
  QString getElfName(uint64_t pc);
  QString getFunction(uint64_t pc);
  uint64_t getLineNumber(uint64_t pc);
  bool isBb(uint64_t pc);
  QString getModuleId(uint64_t pc);

  // get symbol value
  uint64_t lookupSymbol(QString symbol);
};

#endif
