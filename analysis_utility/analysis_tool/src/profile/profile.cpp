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

#include <QTextStream>
#include <QtWidgets>
#include <QTreeView>
#include <QMainWindow>

#include "profile.h"
#include "cfg/loop.h"

Profile::Profile() {
}

Profile::~Profile() {
  clear();
  disconnect();
}

void Profile::connect() {
  static int dbCounter = 0;
  dbConnection = QString("profile") + QString("%1").arg(dbCounter++);

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", dbConnection);
  db.setDatabaseName("profile.db3");

  bool success = db.open();

  if(!success) {
    QSqlError error = db.lastError();
    printf("Can't open DB: %s\n", error.text().toUtf8().constData());
    assert(0);
  }

  QSqlQuery query(db);

  success = query.exec("CREATE TABLE IF NOT EXISTS measurements (time INT, timeSinceLast INT, pc1 INT, pc2 INT, pc3 INT, pc4 INT, basicblock1 TEXT, module1 TEXT, basicblock2 TEXT, module2 TEXT, basicblock3 TEXT, module3 TEXT, basicblock4 TEXT, module4 TEXT, power1 REAL, power2 REAL, power3 REAL, power4 REAL, power5 REAL, power6 REAL, power7 REAL)");
  assert(success);

  success = query.exec("CREATE TABLE IF NOT EXISTS location ("
                       "id INTEGER PRIMARY KEY, core INT, basicblock TEXT, function TEXT, module TEXT, "
                       "runtime REAL, energy1 REAL, energy2 REAL, energy3 REAL, "
                       "energy4 REAL, energy5 REAL, energy6 REAL, energy7 REAL, "
                       "runtimeFrame REAL, energyFrame1 REAL, energyFrame2 REAL, energyFrame3 REAL, "
                       "energyFrame4 REAL, energyFrame5 REAL, energyFrame6 REAL, energyFrame7 REAL, "
                       "loopcount INT)");
  assert(success);

  success = query.exec("CREATE TABLE IF NOT EXISTS arc (fromid INT, selfid INT, num INT)");
  assert(success);

  success = query.exec("CREATE TABLE IF NOT EXISTS frames (time INT, delay INT)");
  assert(success);

  success = query.exec("CREATE TABLE IF NOT EXISTS meta ("
                       "samples INT,mintime INT,maxtime INT,"
                       "minpower1 REAL,minpower2 REAL,minpower3 REAL,minpower4 REAL,minpower5 REAL,minpower6 REAL,"
                       "minpower7 REAL,maxpower1 REAL,maxpower2 REAL,maxpower3 REAL,maxpower4 REAL,maxpower5 REAL,"
                       "maxpower6 REAL,maxpower7 REAL,runtime REAL,energy1 REAL,energy2 REAL,energy3 REAL,"
                       "energy4 REAL,energy5 REAL,energy6 REAL,energy7 REAL,"
                       "frameRuntimeMin REAL,frameRuntimeAvg REAL,frameRuntimeMax REAl,"
                       "frameEnergyMin1 REAL,frameEnergyAvg1 REAL,frameEnergyMax1 REAl,"
                       "frameEnergyMin2 REAL,frameEnergyAvg2 REAL,frameEnergyMax2 REAl,"
                       "frameEnergyMin3 REAL,frameEnergyAvg3 REAL,frameEnergyMax3 REAl,"
                       "frameEnergyMin4 REAL,frameEnergyAvg4 REAL,frameEnergyMax4 REAl,"
                       "frameEnergyMin5 REAL,frameEnergyAvg5 REAL,frameEnergyMax5 REAl,"
                       "frameEnergyMin6 REAL,frameEnergyAvg6 REAL,frameEnergyMax6 REAl,"
                       "frameEnergyMin7 REAL,frameEnergyAvg7 REAL,frameEnergyMax7 REAl"
                       ")");
  assert(success);

  update();
}

void Profile::disconnect() {
  {
    QSqlDatabase db = QSqlDatabase::database(dbConnection);
    db.close();
  }
  QSqlDatabase::removeDatabase(dbConnection);
}

void Profile::update() {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);

  QSqlQuery query(db);

  bool success = query.exec("SELECT mintime,maxtime,runtime,energy1,energy2,energy3,energy4,energy5,energy6,energy7 FROM meta");
  Q_UNUSED(success);
  assert(success);

  if(query.next()) {
    cycles = query.value("maxtime").toLongLong() - query.value("mintime").toLongLong();
    runtime = query.value("runtime").toDouble();
    energy[0] = query.value("energy1").toDouble();
    energy[1] = query.value("energy2").toDouble();
    energy[2] = query.value("energy3").toDouble();
    energy[3] = query.value("energy4").toDouble();
    energy[4] = query.value("energy5").toDouble();
    energy[5] = query.value("energy6").toDouble();
    energy[6] = query.value("energy7").toDouble();
  } else {
    cycles = 0;
    runtime = 0;
    for(unsigned i = 0; i < Pmu::sensors; i++) {
      energy[i] = 0;
    }
  }
}

void Profile::addMeasurement(Measurement measurement) {
  measurements.push_back(measurement);

  unsigned core = measurement.core;
  BasicBlock *bb = measurement.bb;
  std::vector<Measurement> *mments = NULL;

  auto it = measurementsPerBb[core].find(bb);

  if(it == measurementsPerBb[core].end()) {
    mments = new std::vector<Measurement>();
    measurementsPerBb[core][bb] = mments;
  } else {
    mments = it->second;
  }

  mments->push_back(measurement);
}

void Profile::clean() {
  clear();

  QSqlDatabase db = QSqlDatabase::database(dbConnection);

  QSqlQuery query = QSqlQuery(db);
  query.exec("DELETE FROM measurements");
  query.exec("DELETE FROM location");
  query.exec("DELETE FROM arc");
  query.exec("DELETE FROM frames");
  query.exec("DELETE FROM meta");
}

void Profile::setMeasurements(QVector<Measurement> *measurements) {
  for(unsigned core = 0; core < Pmu::maxCores; core++) {
    measurementsPerBb[core].clear();
  }
  this->measurements.clear();
  // for(int i = 1; i < measurements->size(); i++) {
  //   auto measurement = (*measurements)[i];
  //   addMeasurement(measurement);
  // }
  for(auto m : *measurements) {
    addMeasurement(m);
  }
}

void Profile::getProfData(unsigned core, BasicBlock *bb,
                          double *runtime, double *energy, double *runtimeFrame, double *energyFrame, uint64_t *count) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString;

  if(bb->getTop()->externalMod == bb->getModule()) {
    queryString =
      QString() +
      "SELECT id,runtime,energy1,energy2,energy3,energy4,energy5,energy6,energy7,runtimeFrame,energyFrame1,energyFrame2,energyFrame3,energyFrame4,energyFrame5,energyFrame6,energyFrame7,loopcount" +
      " FROM location" +
      " WHERE core = " + QString::number(core) +
      " AND module = \"" + bb->getTop()->externalMod->id +
      "\" AND function = \"" + bb->getFunction()->id + "\"";

  } else {
    queryString =
      QString() +
      "SELECT id,runtime,energy1,energy2,energy3,energy4,energy5,energy6,energy7,runtimeFrame,energyFrame1,energyFrame2,energyFrame3,energyFrame4,energyFrame5,energyFrame6,energyFrame7,loopcount" +
      " FROM location" +
      " WHERE core = " + QString::number(core) +
      " AND module = \"" + bb->getModule()->id +
      "\" AND basicblock = \"" + bb->id + "\"";
  }

  query.exec(queryString);

  if(query.next()) {
    *runtime = query.value("runtime").toDouble();
    energy[0] = query.value("energy1").toDouble();
    energy[1] = query.value("energy2").toDouble();
    energy[2] = query.value("energy3").toDouble();
    energy[3] = query.value("energy4").toDouble();
    energy[4] = query.value("energy5").toDouble();
    energy[5] = query.value("energy6").toDouble();
    energy[6] = query.value("energy7").toDouble();

    *runtimeFrame = query.value("runtimeFrame").toDouble();
    energyFrame[0] = query.value("energyFrame1").toDouble();
    energyFrame[1] = query.value("energyFrame2").toDouble();
    energyFrame[2] = query.value("energyFrame3").toDouble();
    energyFrame[3] = query.value("energyFrame4").toDouble();
    energyFrame[4] = query.value("energyFrame5").toDouble();
    energyFrame[5] = query.value("energyFrame6").toDouble();
    energyFrame[6] = query.value("energyFrame7").toDouble();

    int id = query.value("id").toInt();

    QSqlQuery countQuery(db);
    countQuery.exec("SELECT sum(num) FROM arc WHERE selfid = " + QString::number(id));
    if(countQuery.next()) {
      *count = countQuery.value(0).toInt();
    } else {
      *count = 0; // todo
    }

    // TODO: should possibly be somewhere else
    uint64_t loopCount = query.value("loopcount").toULongLong();
    if(loopCount) {
      Vertex *loop = bb->parent;
      while(!loop->isLoop()) {
        loop = loop->parent;
        if(!loop) break;
      }

      if(loop) (static_cast<Loop*>(loop))->count = loopCount;
      else {
        printf("Cant find loop:\n");
        printf("  Mod %s\n", bb->getModule()->id.toUtf8().constData());
        printf("  Func %s\n", bb->getFunction()->id.toUtf8().constData());
        printf("  Bb: %s\n", bb->id.toUtf8().constData());
      }
    }

  } else {
    *runtime = 0;
    *runtimeFrame = 0;
    for(unsigned i = 0; i < Pmu::sensors; i++) {
      energy[i] = 0;
      energyFrame[i] = 0;
    }
    *count = 0;
  }
}

int Profile::getId(unsigned core, BasicBlock *bb) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString;

  if(bb->getTop()->externalMod == bb->getModule()) {
    queryString =
      QString() +
      "SELECT id FROM location" +
      " WHERE core = " + QString::number(core) +
      " AND module = \"" + bb->getTop()->externalMod->id +
      "\" AND function = \"" + bb->getFunction()->id + "\"";

  } else {
    queryString =
      QString() +
      "SELECT id FROM location" +
      " WHERE core = " + QString::number(core) +
      " AND module = \"" + bb->getModule()->id +
      "\" AND basicblock = \"" + bb->id + "\"";
  }

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toInt();
  } else {
    return 0;
  }
}

double Profile::getArcRatio(unsigned core, BasicBlock *bb, Function *func) {
  int fromid = getId(core, bb);
  int selfid = getId(core, func->getFirstBb());

  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  query.exec("SELECT sum(num) FROM arc WHERE selfid = " + QString::number(selfid));

  int totalCalls = 0;
  if(query.next()) {
    totalCalls = query.value(0).toInt();
  }

  if(!totalCalls) return 0;

  query.exec("SELECT sum(num) FROM arc WHERE fromid = " + QString::number(fromid) + " AND selfid = " + QString::number(selfid));

  int calls = 0;
  if(query.next()) {
    calls = query.value(0).toInt();
  }

  return (double)calls / (double)totalCalls;
}

void Profile::getMeasurements(unsigned core, BasicBlock *bb, QVector<Measurement> *measurements) {
  std::vector<Measurement> *mments;
  auto it = measurementsPerBb[core].find(bb);
  if(it == measurementsPerBb[core].end()) {
    return;
  } else {
    mments = it->second;
  }

  for(auto m : *mments) {
    measurements->push_back(m);
  }
}

void Profile::addExternalFunctions(Cfg *cfg) {
  Module *mod = cfg->externalMod;

  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);

  QString queryString =
    QString() +
    "SELECT function,basicblock" +
    " FROM location" +
    " WHERE module = \"" + mod->id + "\"";

  query.exec(queryString);

  while(query.next()) {
    QString funcId = query.value("function").toString();
    QString bbId = query.value("basicblock").toString();
    Function *func = mod->getFunctionById(funcId);
    if(!func) {
      func = new Function(funcId, mod, mod->children.size());
      mod->appendChild(func);

      BasicBlock *bb = new BasicBlock(bbId, func, 0);
      func->appendChild(bb);
    }
  }
}

void Profile::clear() {
  for(unsigned core = 0; core < Pmu::maxCores; core++) {
    for(auto const &it : measurementsPerBb[core]) {
      delete it.second;
    }
    measurementsPerBb[core].clear();
  }
}

double Profile::getMinPower(unsigned sensor) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT minpower" + QString::number(sensor+1) + " FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

double Profile::getMaxPower(unsigned sensor) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT maxpower" + QString::number(sensor+1) + " FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

double Profile::getFrameRuntimeMin() {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT frameRuntimeMin FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

double Profile::getFrameRuntimeAvg() {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT frameRuntimeAvg FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

double Profile::getFrameRuntimeMax() {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT frameRuntimeMax FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

double Profile::getFrameEnergyMin(unsigned sensor) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT frameEnergyMin" + QString::number(sensor+1) + " FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

double Profile::getFrameEnergyAvg(unsigned sensor) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT frameEnergyAvg" + QString::number(sensor+1) + " FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

double Profile::getFrameEnergyMax(unsigned sensor) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  QString queryString = QString() + "SELECT frameEnergyMax" + QString::number(sensor+1) + " FROM meta";

  query.exec(queryString);

  if(query.next()) {
    return query.value(0).toDouble();
  }
  return 0;
}

bool Profile::exportMeasurements(QString fileName, Cfg *cfg) {
  QFile csvFile(fileName);
  bool success = csvFile.open(QIODevice::WriteOnly);
  if(!success) return false;

  QString header =
    "Time;Power 1;Power 2;Power 3;Power 4;Power 5;Power 6;Power 7;"
    "pc1;pc2;pc3;pc4\n";

  csvFile.write(header.toUtf8());
  
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  
  success = query.exec(QString() + "SELECT mintime FROM meta");
  if(!query.next()) {
    csvFile.close();
    return false;
  }
  int64_t minTime = query.value("mintime").toDouble();

  success = query.exec("SELECT "
                       "time,"
                       "power1,power2,power3,power4,power5,power6,power7,"
                       "pc1,pc2,pc3,pc4 "
                       "FROM measurements");
  assert(success);

  while(query.next()) {
    QString measurement;

    double time = Pmu::cyclesToSeconds(query.value("time").toLongLong() - minTime);
    measurement += QString::number(time);

    for(unsigned sensor = 0; sensor < Pmu::sensors; sensor++) {
      double power = query.value("power" + QString::number(sensor+1)).toDouble();
      measurement += ";" + QString::number(power);
    }

    for(unsigned core = 0; core < Pmu::maxCores; core++) {
      uint64_t pc = query.value("pc" + QString::number(core+1)).toULongLong() << 2;
      measurement += ";" + QString::number(pc);
    }

    measurement += "\n";

    csvFile.write(measurement.toUtf8());
  }

  csvFile.close();

  return true;
}
