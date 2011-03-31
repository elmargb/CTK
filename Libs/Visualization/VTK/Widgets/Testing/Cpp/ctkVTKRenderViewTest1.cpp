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

// Qt includes
#include <QApplication>
#include <QDebug>
#include <QTimer>

// VTK includes
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

// CTK includes
#include "ctkVTKRenderView.h"
#include "ctkCommandLineParser.h"

// STD includes
#include <iostream>

// Convenient macro
#define VTK_CREATE(type, name) \
  vtkSmartPointer<type> name = vtkSmartPointer<type>::New()

//-----------------------------------------------------------------------------
int ctkVTKRenderViewTest1(int argc, char * argv [] )
{
  QApplication app(argc, argv);

  // Command line parser
  ctkCommandLineParser parser;
  parser.addArgument("", "-I", QVariant::Bool);
  QHash<QString, QVariant> parsedArgs = parser.parseArguments(app.arguments());
  bool interactive = parsedArgs["-I"].toBool();

  // Instanciate widget
  ctkVTKRenderView renderView;
  renderView.setBackgroundColor(QColor(Qt::red));
  renderView.setCornerAnnotationText("CTK Rocks !");
  renderView.show();

  // Instanciate VTK objects
  VTK_CREATE(vtkSphereSource, sphere);
  VTK_CREATE(vtkPolyDataMapper, sphereMapper);
  VTK_CREATE(vtkActor, sphereActor);

  // Configure actor
  sphere->SetRadius(0.25);
  sphereMapper->SetInputConnection(sphere->GetOutputPort());
  sphereActor->SetMapper(sphereMapper);

  // Add actor
  renderView.renderer()->AddActor(sphereActor);

  renderView.lookFromAxis(ctkAxesWidget::Right);
  renderView.lookFromAxis(ctkAxesWidget::Left, 10);
  renderView.lookFromAxis(ctkAxesWidget::Anterior, 1.);
  renderView.lookFromAxis(ctkAxesWidget::Posterior, 1.);
  renderView.lookFromAxis(ctkAxesWidget::Superior, 0.333333);
  renderView.lookFromAxis(ctkAxesWidget::Inferior, 0.333333);
  renderView.lookFromAxis(ctkAxesWidget::None, 100.);

  if (!interactive)
    {
    QTimer::singleShot(200, &app, SLOT(quit()));
    }
  return app.exec();
}
