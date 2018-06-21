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

#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>

#include <QApplication>
#include <QTextStream>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>

#include "analysis_tool.h"
#include "project.h"

///////////////////////////////////////////////////////////////////////////////
// makefile creation

void Project::writeCompileRule(QString compiler, QFile &makefile, QString path, QString opt) {
  QFileInfo fileInfo(path);

  QString clangTarget = ultrascale ? A53_CLANG_TARGET : A9_CLANG_TARGET;

  QStringList options;

  options << opt.split(' ');
  options << clangTarget;

  for(auto include : systemIncludes) {
    if(include.trimmed() != "") {
      options << QString("-I") + include;
    }
  }

  options << QString("-I") + this->path + "/src";

  makefile.write((fileInfo.baseName() + ".o : " + path + "\n").toUtf8());
  makefile.write((QString("\t") + compiler + " " + options.join(' ') + " -c $< -o $@\n\n").toUtf8());
}

void Project::writeTulippCompileRule(QString compiler, QFile &makefile, QString path, QString opt) {
  QFileInfo fileInfo(path);

  QString clangTarget = A9_CLANG_TARGET;
  QString llcTarget = A9_LLC_TARGET;
  QString asTarget = A9_AS_TARGET;

  if(ultrascale) {
    clangTarget = A53_CLANG_TARGET;
    llcTarget = A53_LLC_TARGET;
    asTarget = A53_AS_TARGET;
  }

  // .ll
  {
    QStringList options;

    options << opt.split(' ');

    if(cfgOptLevel >= 0) {
      options << QString("-O") + QString::number(cfgOptLevel);
    } else {
      options << QString("-Os");
    }
    options << clangTarget;

    for(auto include : systemIncludes) {
      if(include.trimmed() != "") {
        options << QString("-I") + include;
      }
    }

    options << QString("-I") + this->path + "/src";

    makefile.write((fileInfo.baseName() + ".ll : " + path + "\n").toUtf8());
    makefile.write((QString("\t") + compiler + " " + options.join(' ') + " -g -emit-llvm -S $<\n\n").toUtf8());
  }

  // .xml
  {
    makefile.write((fileInfo.baseName() + ".xml : " + fileInfo.baseName() + ".ll\n").toUtf8());
    makefile.write((QString("\t") + Config::llvm_ir_parser + " $< -xml $@\n\n").toUtf8());
  }

  // _2.ll
  {
    makefile.write((fileInfo.baseName() + "_2.ll : " + fileInfo.baseName() + ".ll\n").toUtf8());
    makefile.write((QString("\t") + Config::llvm_ir_parser + " $< -ll $@\n\n").toUtf8());
  }

  // .s
  {
    QStringList options;
    if(cppOptLevel >= 0) {
      options << QString("-O") + QString::number(cppOptLevel);
    } else {
      options << QString("-Os");
    }
    options << QString(llcTarget).split(' ');

    makefile.write((fileInfo.baseName() + ".s : " + fileInfo.baseName() + "_2.ll\n").toUtf8());
    makefile.write((QString("\t") + Config::llc + " " + options.join(' ') + " $< -o $@\n\n").toUtf8());
  }

  // .o
  bool buildIt = true;
  for(auto acc : accelerators) {
    QFileInfo info(acc.filepath);
    if(info.baseName() == fileInfo.baseName()) {
      buildIt = false;
      break;
    }
  }
  if(buildIt) {
    QStringList options;
    options << QString(asTarget).split(' ');

    makefile.write((fileInfo.baseName() + ".o : " + fileInfo.baseName() + ".s\n").toUtf8());
    makefile.write((QString("\t") + Config::as + " " + options.join(' ') + " $< -o $@\n\n").toUtf8());
  }

  makefile.write(QString("###############################################################################\n\n").toUtf8());
}

void Project::writeLinkRule(QString linker, QFile &makefile, QStringList objects) {
  makefile.write((name + ".elf : " + objects.join(' ') + "\n").toUtf8());
  makefile.write(QString("\t" + linker + " $^ " + linkerOptions + " -o $@\n\n").toUtf8());

  makefile.write(QString("###############################################################################\n\n").toUtf8());
}

bool Project::createXmlMakefile() {
  QStringList xmlFiles;

  QFile makefile("Makefile");
  bool success = makefile.open(QIODevice::WriteOnly);
  if(!success) {
    QMessageBox msgBox;
    msgBox.setText("Can't create Makefile");
    msgBox.exec();
    return false;
  }

  makefile.write(QString("###################################################\n").toUtf8());
  makefile.write(QString("# Autogenerated Makefile for TULIPP Analysis Tool #\n").toUtf8());
  makefile.write(QString("###################################################\n\n").toUtf8());

  makefile.write(QString(".PHONY : clean\n").toUtf8());
  makefile.write(QString("clean :\n").toUtf8());
  makefile.write(QString("\trm -rf *.ll *.xml *.s *.o *.elf *.bit sd_card _sds __tulipp__.* profile.prof __tulipp_test__.* .Xil\n\n").toUtf8());

  makefile.write(QString("###############################################################################\n\n").toUtf8());

  for(auto source : sources) {
    QFileInfo info(source);
    if(info.suffix() == "c") {
      writeTulippCompileRule(Config::clang, makefile, source, cOptions + " " + cSysInc);
      xmlFiles << info.baseName() + ".xml";
    } else if((info.suffix() == "cpp") || (info.suffix() == "cc")) {
      writeTulippCompileRule(Config::clangpp, makefile, source, cppOptions + " " + cppSysInc);
      xmlFiles << info.baseName() + ".xml";
    }
  }

  makefile.write(QString(".PHONY : xml\n").toUtf8());
  makefile.write((QString("xml : ") + xmlFiles.join(' ') + "\n").toUtf8());
  makefile.write(QString("\trm -rf profile.prof\n\n").toUtf8());

  makefile.write(QString("###############################################################################\n\n").toUtf8());

  makefile.close();

  return true;
}

void Project::writeCleanRule(QFile &makefile) {
  makefile.write(QString(".PHONY : clean\n").toUtf8());
  makefile.write(QString("clean :\n").toUtf8());
  makefile.write(QString("\trm -rf *.ll *.xml *.s *.o *.elf *.bit sd_card _sds __tulipp__.* profile.prof __tulipp_test__.* .Xil\n\n").toUtf8());

  makefile.write(QString("###############################################################################\n\n").toUtf8());
}

bool Project::createMakefile(QFile &makefile) {
  makefile.write(QString("###################################################\n").toUtf8());
  makefile.write(QString("# Autogenerated Makefile for TULIPP Analysis Tool #\n").toUtf8());
  makefile.write(QString("###################################################\n\n").toUtf8());

  makefile.write(QString(".PHONY : binary\n").toUtf8());
  makefile.write((QString("binary : ") + name + ".elf\n\n").toUtf8());

  makefile.write(QString("###############################################################################\n\n").toUtf8());

  writeCleanRule(makefile);

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// make

void Project::makeXml() {
  emit advance(0, "Building XML");

  bool created = createXmlMakefile();
  if(created) errorCode = system(QString("make -j xml").toUtf8().constData());
  else errorCode = 1;

  if(!created || errorCode) {
    emit finished(errorCode, "Can't make XML");
  } else {
    emit finished(errorCode, "");
  }
}

void Project::makeBin() {
  emit advance(0, "Building XML");

  bool created = createXmlMakefile();
  if(created) errorCode = system(QString("make -j xml").toUtf8().constData());
  else errorCode = 1;

  if(!created || errorCode) {
    emit finished(errorCode, "Can't make XML");
    return;
  }

  loadFiles();

  emit advance(1, "Building binary");

  created = createMakefile();
  if(created) errorCode = system(QString("make -j binary").toUtf8().constData());
  else errorCode = 1;

  if(!created || errorCode) {
    emit finished(errorCode, "Can't make binary");
  } else {
    emit finished(errorCode, "");
  }
}

void Project::clean() {
  if(opened) {
    createXmlMakefile();
    errorCode = system("make clean");
  }
}

///////////////////////////////////////////////////////////////////////////////

void Project::loadProjectFile() {
  QSettings settings("project.ini", QSettings::IniFormat);

  cfgOptLevel = settings.value("cfgOptLevel", "-1").toInt();
  createBbInfo = settings.value("createBbInfo", true).toBool();
  systemIncludes = settings.value("systemIncludeDirs", Config::defaultSystemIncludeDirs).toStringList();
  systemXmls = settings.value("systemXmls", Config::defaultSystemXmls).toStringList();
  for(unsigned i = 0; i < pmu.numSensors(); i++) {
    pmu.supplyVoltage[i] = settings.value("supplyVoltage" + QString::number(i), 5).toDouble();
  }
  pmu.rl[0] = settings.value("rl0", 0.025).toDouble();
  pmu.rl[1] = settings.value("rl1", 0.05).toDouble();
  pmu.rl[2] = settings.value("rl2", 0.05).toDouble();
  pmu.rl[3] = settings.value("rl3", 0.1).toDouble();
  pmu.rl[4] = settings.value("rl4", 0.1).toDouble();
  pmu.rl[5] = settings.value("rl5", 1).toDouble();
  pmu.rl[6] = settings.value("rl6", 10).toDouble();
  ultrascale = settings.value("ultrascale", true).toBool();
  tcfUploadScript = settings.value("tcfUploadScript",
                                   "connect -url tcp:127.0.0.1:3121\n"
                                   "source /opt/Xilinx/SDx/2017.2/SDK/scripts/sdk/util/zynqmp_utils.tcl\n"
                                   "targets 8\n"
                                   "rst -system\n"
                                   "after 3000\n"
                                   "targets 8\n"
                                   "while {[catch {fpga -file $name.elf.bit}] eq 1} {rst -system}\n"
                                   "targets 8\n"
                                   "source _sds/p0/ipi/emc2_hdmi_ultra.sdk/psu_init.tcl\n"
                                   "psu_init\n"
                                   "after 1000\n"
                                   "psu_ps_pl_isolation_removal\n"
                                   "after 1000\n"
                                   "psu_ps_pl_reset_config\n"
                                   "catch {psu_protection}\n"
                                   "targets 9\n"
                                   "rst -processor\n"
                                   "dow sd_card/$name.elf\n").toString();
  useCustomElf = settings.value("useCustomElf", false).toBool();
  customElfFile = settings.value("customElfFile", "").toString();
  startFunc = settings.value("startFunc", "main").toString();
  startCore = settings.value("startCore", 0).toUInt();
  stopFunc = settings.value("stopFunc", "_exit").toString();
  stopCore = settings.value("stopCore", 0).toUInt();

  if(!isSdSocProject()) {
    sources = settings.value("sources").toStringList();
    cOptLevel = settings.value("cOptLevel", 0).toInt();
    cOptions = settings.value("cOptions", "").toString();
    cppOptLevel = settings.value("cppOptLevel", 0).toInt();
    cppOptions = settings.value("cppOptions", "").toString();
    linkerOptions = settings.value("linkerOptions", "").toString();
  }
}

void Project::saveProjectFile() {
  QSettings settings("project.ini", QSettings::IniFormat);

  settings.setValue("cfgOptLevel", cfgOptLevel);
  settings.setValue("createBbInfo", createBbInfo);
  settings.setValue("systemIncludeDirs", systemIncludes);
  settings.setValue("systemXmls", systemXmls);
  for(unsigned i = 0; i < pmu.numSensors(); i++) { 
    settings.setValue("supplyVoltage" + QString::number(i), pmu.supplyVoltage[i]);
    settings.setValue("rl" + QString::number(i), pmu.rl[i]);
  }
  settings.setValue("ultrascale", ultrascale);
  settings.setValue("tcfUploadScript", tcfUploadScript);
  settings.setValue("useCustomElf", useCustomElf);
  settings.setValue("customElfFile", customElfFile);
  settings.setValue("startFunc", startFunc);
  settings.setValue("startCore", startCore);
  settings.setValue("stopFunc", stopFunc);
  settings.setValue("stopCore", stopCore);

  settings.setValue("sources", sources);
  settings.setValue("cOptLevel", cOptLevel);
  settings.setValue("cOptions", cOptions);
  settings.setValue("cppOptLevel", cppOptLevel);
  settings.setValue("cppOptions", cppOptions);
  settings.setValue("linkerOptions", linkerOptions);
}

///////////////////////////////////////////////////////////////////////////////
// transform source

int Project::runSourceTool(QString inputFilename, QString outputFilename, QStringList loopsToPipeline) {
  QStringList options;

  options << inputFilename;

  for(auto x : cOptions.split(' ')) {
    if(x.trimmed() != "") {
      options << QString("-extra-arg=") + x;
    }
  }

  for(auto include : systemIncludes) {
    if(include.trimmed() != "") {
      options << QString("-extra-arg=-I") + include;
    }
  }

  options << QString("-extra-arg=-I") + this->path + "/src";

  for(auto l : loopsToPipeline) {
    options << QString("-pipeloop=") + l;
  }

  options << QString("-output " + outputFilename);

  options << QString("--");

  return system((Config::tulipp_source_tool + " " + options.join(' ')).toUtf8().constData());
}

///////////////////////////////////////////////////////////////////////////////
// prof file parsing

QVector<Measurement> *Project::parseProfFile() {
  QFile file("profile.prof");
  int ret = file.open(QIODevice::ReadOnly);
  if(ret) {
    QVector<Measurement> *measurements = parseProfFile(file);
    file.close();
    return measurements;
  } else {
    return new QVector<Measurement>;
  }
}

QVector<Measurement> *Project::parseProfFile(QFile &file) {
  QVector<Measurement> *measurements = new QVector<Measurement>;

  while(!file.atEnd()) {
    Measurement m;
    m.read(file, cfgModel->getCfg());
    measurements->push_back(m);
  }

  return measurements;
}

///////////////////////////////////////////////////////////////////////////////
// project files

void Project::loadFiles() {
  if(cfgModel) delete cfgModel;
  cfgModel = new CfgModel();

  QDir dir(".");
  dir.setFilter(QDir::Files);

  // read system XML files
  for(auto filename : systemXmls) {
    if(filename != "") loadXmlFile(filename);
  }

  // read XML files from tulipp project dir
  {
    QStringList nameFilter;
    nameFilter << "*.xml";
    dir.setNameFilters(nameFilter);

    QFileInfoList list = dir.entryInfoList();
    for(auto fileInfo : list) {
      loadXmlFile(fileInfo.filePath());
    }
  }
}

void Project::loadXmlFile(const QString &fileName) {
  QDomDocument doc;
  QFile file(fileName);
  if(!file.open(QIODevice::ReadOnly)) {
    QMessageBox msgBox;
    msgBox.setText("File not found");
    msgBox.exec();
    return;
  }
  if(!doc.setContent(&file)) {
    QMessageBox msgBox;
    msgBox.setText("Invalid XML file");
    msgBox.exec();
    file.close();
    return;
  }
  file.close();

  try {
    cfgModel->addModule(doc, *this);
  } catch (std::exception &e) {
    QMessageBox msgBox;
    msgBox.setText("Invalid CFG file");
    msgBox.exec();
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////
// object construction, destruction and management

Project::Project() {
  cfgModel = NULL;
  close();
}

void Project::copy(Project *p) {
  opened = p->opened;
  isCpp = p->isCpp;

  path = p->path;
  sources = p->sources;
  name = p->name;
  configType = p->configType;
  accelerators = p->accelerators;
  
  cOptions = p->cOptions;
  cOptLevel = p->cOptLevel;
  cppOptions = p->cppOptions;
  cppOptLevel = p->cppOptLevel;
  linkerOptions = p->linkerOptions;
  
  elfFile = p->elfFile;

  cfgModel = NULL;
}

Project::Project(Project *p) {
  copy(p);
}

Project::~Project() {
  delete cfgModel;
}

void Project::close() {
  opened = false;
  isCpp = false;
  path = "";
  clear();
}

void Project::clear() {
  sources.clear();
  accelerators.clear();
  if(cfgModel) delete cfgModel;
  cfgModel = new CfgModel();
}

void Project::print() {
  printf("Project: %s\n", name.toUtf8().constData());
  printf("Path: %s\n", path.toUtf8().constData());

  printf("Files:\n");
  for(auto source : sources) {
    printf("  %s\n", source.toUtf8().constData());
  }

  printf("Accelerators:\n");
  for(auto acc : accelerators) {
    acc.print();
  }

  printf("C options: %s\n", cOptions.toUtf8().constData());
  printf("C opt level: %d\n", cOptLevel);
  printf("C++ options: %s\n", cppOptions.toUtf8().constData());
  printf("C++ opt level: %d\n", cppOptLevel);
  printf("Linker options: %s\n", linkerOptions.toUtf8().constData());
}

///////////////////////////////////////////////////////////////////////////////
// profiling support

void Project::runProfiler() {

  ElfSupport elfSupport(elfFile);
  if(useCustomElf) {
    elfSupport = ElfSupport(customElfFile);
  }

  bool pmuInited = pmu.init();
  if(!pmuInited) {
    emit finished(1, "Can't connect to PMU");
    return;
  }

  emit advance(0, "Uploading binary");

  // upload binaries
  QFile tclFile("temp-pmu-prof.tcl");
  bool success = tclFile.open(QIODevice::WriteOnly);
  Q_UNUSED(success);
  assert(success);

  QString tcl = QString() + "set name " + name + "\n" + tcfUploadScript;
      
  tclFile.write(tcl.toUtf8());

  tclFile.close();

  int ret = system("xsct temp-pmu-prof.tcl");
  if(ret) {
    emit finished(1, "Can't upload binaries");
    pmu.release();
    return;
  }

  // collect samples
  emit advance(1, "Collecting samples");

  QFile profFile("profile.prof");
  success = profFile.open(QIODevice::WriteOnly);
  Q_UNUSED(success);
  assert(success);

  uint8_t *buf = (uint8_t*)malloc(SAMPLEBUF_SIZE);
  assert(buf);

  pmu.collectSamples(buf, SAMPLEBUF_SIZE, startCore, elfSupport.lookupSymbol(startFunc), stopCore, elfSupport.lookupSymbol(stopFunc));

  emit advance(2, "Processing samples");

  Measurement m[pmu.numCores()];

  while(pmu.getNextSample(m)) {
    for(unsigned core = 0; core < pmu.numCores(); core++) {
      if(!useCustomElf && elfSupport.isBb(m[core].pc)) {
        Module *mod = cfgModel->getCfg()->getModuleById(elfSupport.getModuleId(m[core].pc));
        m[core].bb = NULL;
        if(mod) {
          m[core].bb = mod->getBasicBlockById(QString::number(elfSupport.getLineNumber(m[core].pc)));
          m[core].write(profFile);
        }

      } else {
        Function *func = cfgModel->getCfg()->getFunctionById(elfSupport.getFunction(m[core].pc));

        if(func) {
          // we don't know the BB, but the function exists in the CFG: add to first BB
          m[core].bb = func->getFirstBb();
          if(m[core].bb) {
            m[core].write(profFile);
          }
          
        } else {
          // the function does not exist in the CFG
          Module *mod = cfgModel->getCfg()->externalMod;
          m[core].func = mod->getFunctionById(elfSupport.getFunction(m[core].pc));
          m[core].bb = NULL;
          if(m[core].func) {
            m[core].bb = static_cast<BasicBlock*>(m[core].func->children[0]);
          } else {
            m[core].func = new Function(elfSupport.getFunction(m[core].pc), mod, mod->children.size());
            mod->appendChild(m[core].func);

            m[core].bb = new BasicBlock(QString::number(mod->children.size()), m[core].func, 0);
            m[core].func->appendChild(m[core].bb);
          }
          m[core].write(profFile);
        }
      }
    }
  }

  profFile.close();

  pmu.release();

  emit finished(0, "");
}

///////////////////////////////////////////////////////////////////////////////

Cfg *Project::getCfg() {
  return cfgModel->getCfg();
}

