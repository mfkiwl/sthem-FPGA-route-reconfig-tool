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

#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QProcess>

#include "elfsupport.h"

static char *readLine(char *s, int size, FILE *stream) {
  char *ret = NULL;

  while(!ret) {
    ret = fgets(s, size, stream);
    if(feof(stream)) return NULL;
    if(ferror(stream)) {
      if(errno == EINTR) clearerr(stream);
      else return NULL;
    }
  }

  return ret;
}


void ElfSupport::setPc(uint64_t pc) {
  if(prevPc != pc) {
    prevPc = pc;

    // check if pc exists in cache
    auto it = addr2lineCache.find(pc);
    if(it != addr2lineCache.end()) {
      addr2line = (*it).second;
      return;
    }

    // check if pc exists in elf files
    for(auto elfFile : elfFiles) {
      QString fileName = "";
      QString function = "Unknown";
      uint64_t lineNumber = 0;

      if(!elfFile.trimmed().isEmpty()) {
        char buf[1024];
        FILE *fp;
        std::stringstream pcStream;
        std::string cmd;

        Offset elfOffset = elfOffsets.value(QFileInfo(elfFile).fileName());
        uint64_t offset = elfOffset.offset;
        uint64_t end = offset + elfOffset.size;

        if((pc < offset) || (pc > end)) continue;

        if(isStatic(elfFile)) {
          offset = 0;
        }

        // create command
        pcStream << std::hex << (pc - offset);
        cmd = "addr2line -C -f -a " + pcStream.str() + " -e " + elfFile.toUtf8().constData();

        // run addr2line program
        if((fp = popen(cmd.c_str(), "r")) == NULL) goto error;

        // discard first output line
        if(readLine(buf, 1024, fp) == NULL) goto error;

        // get function name
        if(readLine(buf, 1024, fp) == NULL) goto error;
        function = QString::fromUtf8(buf).simplified();
        if(function == "??") function = "Unknown";

        // get filename and linenumber
        if(readLine(buf, 1024, fp) == NULL) goto error;

        {
          QString qbuf = QString::fromUtf8(buf);
          fileName = qbuf.left(qbuf.indexOf(':'));
          lineNumber = qbuf.mid(qbuf.indexOf(':') + 1).toULongLong();
        }

        // close stream
        if(pclose(fp)) goto error;
      }

      addr2line = Addr2Line(fileName, elfFile, function, lineNumber);
      addr2lineCache[pc] = addr2line;

      if((function != "Unknown") || (lineNumber != 0)) return;
    }

    // check if pc exists in kallsyms file
    if(!symsFile.trimmed().isEmpty()) {
      QFile file(symsFile);
      if(file.open(QIODevice::ReadOnly)) {

        QString lastSymbol;
        quint64 lastAddress = ~0;

        while(!file.atEnd()) {
          QString line = file.readLine();

          QStringList tokens = line.split(' ');

          if(tokens.size() >= 3) {
            quint64 address = tokens[0].toULongLong(nullptr, 16);
            //char symbolType = tokens[1][0].toLatin1();
            QString symbol = tokens[2].trimmed();

            if(pc < address) {
              if(lastAddress < pc) {
                addr2line = Addr2Line("", "kallsyms", symbol, 0);
                addr2lineCache[pc] = addr2line;
              }
              file.close();
              return;
            }

            lastSymbol = symbol;
            lastAddress = address;
          }
        }

        file.close();
      }
    }
  }

  return;

 error:
  addr2line = Addr2Line("", "", "", 0);
  addr2lineCache[pc] = addr2line;
}

QString ElfSupport::getFilename(uint64_t pc) {
  setPc(pc);
  return addr2line.filename;
}

QString ElfSupport::getElfName(uint64_t pc) {
  setPc(pc);
  return addr2line.elfName;
}

QString ElfSupport::getFunction(uint64_t pc) {
  setPc(pc);
  return addr2line.function;
}

uint64_t ElfSupport::getLineNumber(uint64_t pc) {
  setPc(pc);
  return addr2line.lineNumber;
}

bool ElfSupport::isBb(uint64_t pc) {
  setPc(pc);
  return addr2line.filename[0] == '@';
}

QString ElfSupport::getModuleId(uint64_t pc) {
  setPc(pc);
  return addr2line.filename.right(addr2line.filename.size()-1);
}

uint64_t ElfSupport::lookupSymbol(QString symbol) {
  FILE *fp;
  char buf[1024];

  for(auto elfFile : elfFiles) {
    Offset elfOffset = elfOffsets.value(QFileInfo(elfFile).fileName());
    uint64_t offset = elfOffset.offset;
    if(isStatic(elfFile)) {
      offset = 0;
    }

    // create command line
    QString cmd = QString("nm ") + elfFile;

    // run program
    if((fp = popen(cmd.toUtf8().constData(), "r")) == NULL) goto error;

    while(!feof(fp) && !ferror(fp)) {
      if(readLine(buf, 1024, fp)) {
        QStringList line = QString::fromUtf8(buf).split(' ');
        if(line[2].trimmed() == symbol) {
          return line[0].toULongLong(0, 16) + offset;
        }
      }
    }
  }

 error:
  return 0;
}

void ElfSupport::addElfOffsetsFromFile(QString offsetFile) {
  if(!offsetFile.trimmed().isEmpty()) {
    QFile file(offsetFile);
    if(file.open(QIODevice::ReadOnly)) {
      while(!file.atEnd()) {
        QStringList tokens = ((QString)file.readLine()).split(' ');
        if(tokens.size() == 3) {
          uint64_t offset = tokens[0].toULongLong(nullptr, 16);
          uint64_t size = tokens[1].toULongLong(nullptr, 16);
          QString elf = tokens[2].simplified();
          elfOffsets[elf] = Offset(offset, size);
        }
      }
      file.close();
    }
  }
}

bool ElfSupport::isStatic(QString elf) {
  QString elfFileName = QFileInfo(elf).fileName();

  if(elfStatic.find(elfFileName) == elfStatic.end()) {
    QString cmd = "readelf -h " + elf;

    QProcess process;
    process.start(cmd);
    process.waitForFinished(-1);

    QString stdout = process.readAllStandardOutput();
    bool stat = stdout.contains("EXEC");
    elfStatic[elfFileName] = stat;

    return stat;
  }

  return elfStatic[elfFileName];
}
