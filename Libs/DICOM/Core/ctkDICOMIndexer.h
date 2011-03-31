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

#ifndef __ctkDICOMIndexer_h
#define __ctkDICOMIndexer_h

// Qt includes 
#include <QSqlDatabase>

#include "ctkDICOMCoreExport.h"
#include "ctkDICOMDatabase.h"

class ctkDICOMIndexerPrivate;
/**
    \brief Indexes DICOM images located in local directory into an Sql database
*/
class CTK_DICOM_CORE_EXPORT ctkDICOMIndexer
{
public:
  explicit ctkDICOMIndexer();
  virtual ~ctkDICOMIndexer();
  
  /**
      \brief Adds directory to database and optionally copies files to
      destinationDirectory.
      
      Scan the directory using Dcmtk and populate the database with all the
      DICOM images accordingly.
  */
  void addDirectory(ctkDICOMDatabase& database, const QString& directoryName,
                    const QString& destinationDirectoryName = "",
                    bool createHierarchy = true, bool createThumbnails = true);
  void refreshDatabase(ctkDICOMDatabase& database, const QString& directoryName);
protected:
  QScopedPointer<ctkDICOMIndexerPrivate> d_ptr;
  
private:
  Q_DECLARE_PRIVATE(ctkDICOMIndexer);
  Q_DISABLE_COPY(ctkDICOMIndexer);

};

#endif
