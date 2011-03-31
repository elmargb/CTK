/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.commontk.org/LICENSE

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

#ifndef __ctkDICOMImportWidget_h
#define __ctkDICOMImportWidget_h

// Qt includes 
#include <QWidget>

#include "ctkDICOMWidgetsExport.h"

class ctkDICOMImportWidgetPrivate;

class ctkDICOMDatabase;

class CTK_DICOM_WIDGETS_EXPORT ctkDICOMImportWidget : public QWidget
{
  Q_OBJECT
public:
  typedef QWidget Superclass;
  explicit ctkDICOMImportWidget(QWidget* parent=0);
  virtual ~ctkDICOMImportWidget();

  void setDICOMDatabase(QSharedPointer<ctkDICOMDatabase> database);

public slots:
  void setTopDirectory(const QString& path);

protected slots:
  void onTopDirectoryChanged(const QString& path);

protected:
  QScopedPointer<ctkDICOMImportWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(ctkDICOMImportWidget);
  Q_DISABLE_COPY(ctkDICOMImportWidget);

};

#endif
