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

#ifndef __ctkVTKAbstractView_p_h
#define __ctkVTKAbstractView_p_h

// Qt includes
#include <QObject>

// CTK includes
#include "ctkVTKAbstractView.h"

// VTK includes
#include <QVTKWidget.h>
#include <vtkCornerAnnotation.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

//-----------------------------------------------------------------------------
class ctkVTKAbstractViewPrivate : public QObject
{
  Q_OBJECT
  Q_DECLARE_PUBLIC(ctkVTKAbstractView);

protected:
  ctkVTKAbstractView* const q_ptr;

public:
  ctkVTKAbstractViewPrivate(ctkVTKAbstractView& object);

  /// Convenient setup methods
  virtual void init();
  virtual void setupCornerAnnotation();
  virtual void setupRendering();

  QVTKWidget*                                   VTKWidget;
  vtkSmartPointer<vtkRenderWindow>              RenderWindow;
  bool                                          RenderPending;
  bool                                          RenderEnabled;

  vtkSmartPointer<vtkCornerAnnotation>          CornerAnnotation;
};

#endif
