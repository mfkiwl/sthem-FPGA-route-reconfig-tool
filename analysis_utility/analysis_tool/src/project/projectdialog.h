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

#ifndef PROJECTDIALOG_H
#define PROJECTDIALOG_H

#include <QtWidgets>
#include <QDialog>

#include "project.h"

class QListWidget;
class QListWidgetItem;
class QStackedWidget;

///////////////////////////////////////////////////////////////////////////////

class ProjectMainPage : public QWidget {
public:
  QPlainTextEdit *xmlEdit;

  ProjectMainPage(Project *project, QWidget *parent = 0);
};

///////////////////////////////////////////////////////////////////////////////

class ProjectBuildPage : public QWidget {
public:
  QCheckBox *createBbInfoCheckBox;

  QComboBox *optCombo;

  QCheckBox *instrumentCheckBox;

  QLineEdit *cmakeOptions;

  ProjectBuildPage(Project *project, QWidget *parent = 0);
};

///////////////////////////////////////////////////////////////////////////////

class ProjectProfPage : public QWidget {
  Q_OBJECT

public slots:
  void setDefault(bool checked);
  void setDefaultUs(bool checked);
  void setDefaultHipperosUs(bool checked);
  void updateGui();

public:
  QPlainTextEdit *tcfUploadScriptEdit;
  QLineEdit *rlEdit[LYNSYN_SENSORS];
  QLineEdit *supplyVoltageEdit[LYNSYN_SENSORS];

  QCheckBox *samplingModeGpioCheckBox;
  QCheckBox *runTcfCheckBox;
  QCheckBox *samplePcCheckBox;

  QCheckBox *startAtBpCheckBox;
  QLineEdit *startFuncEdit;

  QComboBox *stopAtCombo;
  QLineEdit *stopFuncEdit;
  QLineEdit *samplePeriodEdit;

  QLineEdit *frameFuncEdit;

  QLineEdit *customElfEdit;

  ProjectProfPage(Project *project, QWidget *parent = 0);
};

///////////////////////////////////////////////////////////////////////////////

class ProjectDialog : public QDialog {
  Q_OBJECT

public:
  ProjectDialog(Project *project);

protected:
  virtual void closeEvent(QCloseEvent *e);

public slots:
  void changePage(QListWidgetItem *current, QListWidgetItem *previous);

private:
  ProjectMainPage *mainPage;
  ProjectBuildPage *buildPage;
  ProjectProfPage *profPage;

  QListWidget *contentsWidget;
  QStackedWidget *pagesWidget;

  Project *project;
};

#endif
