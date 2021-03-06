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

#include "function.h"
#include "basicblock.h"
#include "module.h"

BasicBlock *Function::getFirstBb() {
  Vertex *vertex = entryNode->getEdge(0)->target;
  BasicBlock *bb = dynamic_cast<BasicBlock*>(vertex);
  while(!bb) {
    vertex = vertex->getEdge(0)->target;
    bb = dynamic_cast<BasicBlock*>(vertex);
  }
  return bb;
}

QString Function::getTableName() {
  return getModule()->id + ":" + name + "()";
}

QString Function::getCfgName() {
  if(name == "main") {
    return getModule()->id + ":" + name + "()";
  } else {
    return name + "()";
  }
}
