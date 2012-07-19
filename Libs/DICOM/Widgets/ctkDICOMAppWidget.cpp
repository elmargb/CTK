/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

// std includes
#include <iostream>

#include <dcmimage.h>

// Qt includes
#include <QAction>
#include <QCheckBox>
#include <QDebug>
#include <QMetaType>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QProgressDialog>
#include <QSettings>
#include <QSlider>
#include <QSqlDatabase>
#include <QSqlError>
#include <QTabBar>
#include <QTimer>
#include <QTreeView>

// ctkWidgets includes
#include "ctkDirectoryButton.h"
#include "ctkFileDialog.h"

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"
#include "ctkDICOMFilterProxyModel.h"
#include "ctkDICOMIndexer.h"
#include "ctkDICOMModel.h"

// ctkDICOMWidgets includes
#include "ctkDICOMAppWidget.h"
#include "ctkDICOMThumbnailGenerator.h"
#include "ctkThumbnailLabel.h"
#include "ctkDICOMQueryResultsTabWidget.h"
#include "ctkDICOMQueryRetrieveWidget.h"
#include "ctkDICOMQueryWidget.h"

#include "ui_ctkDICOMAppWidget.h"

//logger
#include <ctkLogger.h>
static ctkLogger logger("org.commontk.DICOM.Widgets.ctkDICOMAppWidget");

Q_DECLARE_METATYPE(QPersistentModelIndex);

//----------------------------------------------------------------------------
class ctkDICOMAppWidgetPrivate: public Ui_ctkDICOMAppWidget
{
public:
  ctkDICOMAppWidget* const q_ptr;
  Q_DECLARE_PUBLIC(ctkDICOMAppWidget);

  ctkDICOMAppWidgetPrivate(ctkDICOMAppWidget* );

  ctkFileDialog* ImportDialog;
  ctkDICOMQueryRetrieveWidget* QueryRetrieveWidget;

  QSharedPointer<ctkDICOMDatabase> DICOMDatabase;
  QSharedPointer<ctkDICOMThumbnailGenerator> ThumbnailGenerator;

  ctkDICOMModel DICOMModel;
  ctkDICOMFilterProxyModel DICOMProxyModel;
  QSqlDatabase DICOMModelDatabase;

  QSharedPointer<ctkDICOMIndexer> DICOMIndexer;

  // used when suspending the ctkDICOMModel
  QSqlDatabase EmptyDatabase;

  QTimer* AutoPlayTimer;

  bool IsSearchWidgetPopUpMode;
};

//----------------------------------------------------------------------------
// ctkDICOMAppWidgetPrivate methods

ctkDICOMAppWidgetPrivate::ctkDICOMAppWidgetPrivate(ctkDICOMAppWidget* parent): q_ptr(parent){
  DICOMDatabase = QSharedPointer<ctkDICOMDatabase> (new ctkDICOMDatabase);
  ThumbnailGenerator = QSharedPointer <ctkDICOMThumbnailGenerator> (new ctkDICOMThumbnailGenerator);
  DICOMDatabase->setThumbnailGenerator(ThumbnailGenerator.data());
  DICOMIndexer = QSharedPointer<ctkDICOMIndexer> (new ctkDICOMIndexer);
}

//----------------------------------------------------------------------------
// ctkDICOMAppWidget methods

//----------------------------------------------------------------------------
ctkDICOMAppWidget::ctkDICOMAppWidget(QWidget* _parent):Superclass(_parent), 
    d_ptr(new ctkDICOMAppWidgetPrivate(this))
{
  Q_D(ctkDICOMAppWidget);  

  d->setupUi(this);

  this->setSearchWidgetPopUpMode(false);

  //Hide image previewer buttons
  d->NextImageButton->hide();
  d->PrevImageButton->hide();
  d->NextSeriesButton->hide();
  d->PrevSeriesButton->hide();
  d->NextStudyButton->hide();
  d->PrevStudyButton->hide();

  //Enable sorting in tree view
  d->TreeView->setSortingEnabled(true);
  d->TreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->DICOMProxyModel.setSourceModel(&d->DICOMModel);
  d->TreeView->setModel(&d->DICOMModel);

  d->ThumbnailsWidget->setThumbnailSize(
    QSize(d->ThumbnailWidthSlider->value(), d->ThumbnailWidthSlider->value()));

  connect(d->TreeView, SIGNAL(collapsed(QModelIndex)), this, SLOT(onTreeCollapsed(QModelIndex)));
  connect(d->TreeView, SIGNAL(expanded(QModelIndex)), this, SLOT(onTreeExpanded(QModelIndex)));

  //Set ToolBar button style
  d->ToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

  //Initialize Q/R widget
  d->QueryRetrieveWidget = new ctkDICOMQueryRetrieveWidget();
  d->QueryRetrieveWidget->setWindowModality ( Qt::ApplicationModal );

  //initialize directory from settings, then listen for changes
  QSettings settings;
  if ( settings.value("DatabaseDirectory", "") == "" )
    {
    QString directory = QString("./ctkDICOM-Database");
    settings.setValue("DatabaseDirectory", directory);
    settings.sync();
    }
  QString databaseDirectory = settings.value("DatabaseDirectory").toString();
  this->setDatabaseDirectory(databaseDirectory);
  d->DirectoryButton->setDirectory(databaseDirectory);

  connect(d->DirectoryButton, SIGNAL(directoryChanged(QString)), this, SLOT(setDatabaseDirectory(QString)));

  //Initialize import widget
  d->ImportDialog = new ctkFileDialog();
  QCheckBox* importCheckbox = new QCheckBox("Copy on import", d->ImportDialog);
  d->ImportDialog->setBottomWidget(importCheckbox);
  d->ImportDialog->setFileMode(QFileDialog::Directory);
  d->ImportDialog->setLabelText(QFileDialog::Accept,"Import");
  d->ImportDialog->setWindowTitle("Import DICOM files from directory ...");
  d->ImportDialog->setWindowModality(Qt::ApplicationModal);

  //connect signal and slots
  connect(d->TreeView, SIGNAL(clicked(QModelIndex)), d->ThumbnailsWidget, SLOT(onModelSelected(QModelIndex)));
  connect(d->TreeView, SIGNAL(clicked(QModelIndex)), d->ImagePreview, SLOT(onModelSelected(QModelIndex)));
  connect(d->TreeView, SIGNAL(clicked(QModelIndex)), this, SLOT(onModelSelected(QModelIndex)));

  connect(d->ThumbnailsWidget, SIGNAL(selected(ctkThumbnailLabel)), this, SLOT(onThumbnailSelected(ctkThumbnailLabel)));
  connect(d->ThumbnailsWidget, SIGNAL(doubleClicked(ctkThumbnailLabel)), this, SLOT(onThumbnailDoubleClicked(ctkThumbnailLabel)));
  connect(d->ImportDialog, SIGNAL(fileSelected(QString)),this,SLOT(onImportDirectory(QString)));

  connect(d->QueryRetrieveWidget, SIGNAL(canceled()), d->QueryRetrieveWidget, SLOT(hide()) );
  connect(d->QueryRetrieveWidget, SIGNAL(canceled()), this, SLOT(onQueryRetrieveFinished()) );

  connect(d->ImagePreview, SIGNAL(requestNextImage()), this, SLOT(onNextImage()));
  connect(d->ImagePreview, SIGNAL(requestPreviousImage()), this, SLOT(onPreviousImage()));
  connect(d->ImagePreview, SIGNAL(imageDisplayed(int,int)), this, SLOT(onImagePreviewDisplayed(int,int)));

  connect(d->SearchOption, SIGNAL(parameterChanged()), this, SLOT(onSearchParameterChanged()));

  connect(d->PlaySlider, SIGNAL(valueChanged(int)), d->ImagePreview, SLOT(displayImage(int)));
}

//----------------------------------------------------------------------------
ctkDICOMAppWidget::~ctkDICOMAppWidget()
{
  Q_D(ctkDICOMAppWidget);  

  d->QueryRetrieveWidget->deleteLater();
  d->ImportDialog->deleteLater();
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::setDatabaseDirectory(const QString& directory)
{
  Q_D(ctkDICOMAppWidget);  

  QSettings settings;
  settings.setValue("DatabaseDirectory", directory);
  settings.sync();

  //close the active DICOM database
  d->DICOMDatabase->closeDatabase();
  
  //open DICOM database on the directory
  QString databaseFileName = directory + QString("/ctkDICOM.sql");
  try
    {
    d->DICOMDatabase->openDatabase( databaseFileName );
    }
  catch (std::exception e)
    {
    std::cerr << "Database error: " << qPrintable(d->DICOMDatabase->lastError()) << "\n";
    d->DICOMDatabase->closeDatabase();
    return;
    }
  
  d->DICOMModelDatabase = QSqlDatabase::addDatabase("QSQLITE", "ctkDICOMModelConnection");
  d->DICOMModelDatabase.setDatabaseName(databaseFileName);
  if ( ! (d->DICOMModelDatabase.open()) )
    {
    std::cerr << "Database error: " << qPrintable(d->DICOMModelDatabase.lastError().text()) << "\n";
    }
  d->DICOMModel.setDatabase(d->DICOMModelDatabase);
  d->DICOMModel.setEndLevel(ctkDICOMModel::SeriesType);
  d->TreeView->resizeColumnToContents(0);

  //pass DICOM database instance to Import widget
  // d->ImportDialog->setDICOMDatabase(d->DICOMDatabase);
  d->QueryRetrieveWidget->setRetrieveDatabase(d->DICOMDatabase);

  // update the button and let any connected slots know about the change
  d->DirectoryButton->setDirectory(directory);
  d->ThumbnailsWidget->setDatabaseDirectory(directory);
  d->ImagePreview->setDatabaseDirectory(directory);
  emit databaseDirectoryChanged(directory);
}

//----------------------------------------------------------------------------
QString ctkDICOMAppWidget::databaseDirectory() const
{
  QSettings settings;
  return settings.value("DatabaseDirectory").toString();
}

//----------------------------------------------------------------------------
bool ctkDICOMAppWidget::searchWidgetPopUpMode(){
  Q_D(ctkDICOMAppWidget);
  return d->IsSearchWidgetPopUpMode;
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::setSearchWidgetPopUpMode(bool flag){
  Q_D(ctkDICOMAppWidget);

  if(flag)
    {
    d->SearchDockWidget->setTitleBarWidget(0);
    d->SearchPopUpButton->show();
    d->SearchDockWidget->hide();
    d->SearchDockWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    connect(d->SearchDockWidget, SIGNAL(topLevelChanged(bool)), this, SLOT(onSearchWidgetTopLevelChanged(bool)));
    connect(d->SearchPopUpButton, SIGNAL(clicked()), this, SLOT(onSearchPopUpButtonClicked()));
    }
  else
    {
    d->SearchDockWidget->setTitleBarWidget(new QWidget());
    d->SearchPopUpButton->hide();
    d->SearchDockWidget->show();
    d->SearchDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
    disconnect(d->SearchDockWidget, SIGNAL(topLevelChanged(bool)), this, SLOT(onSearchWidgetTopLevelChanged(bool)));
    disconnect(d->SearchPopUpButton, SIGNAL(clicked()), this, SLOT(onSearchPopUpButtonClicked()));
    }

  d->IsSearchWidgetPopUpMode = flag;
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onAddToDatabase()
{
  //Q_D(ctkDICOMAppWidget);

  //d->
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::openImportDialog()
{
  Q_D(ctkDICOMAppWidget);
  
  d->ImportDialog->show();
  d->ImportDialog->raise();
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::openExportDialog()
{

}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::openQueryDialog()
{
  Q_D(ctkDICOMAppWidget);

  d->QueryRetrieveWidget->show();
  d->QueryRetrieveWidget->raise();

}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onQueryRetrieveFinished()
{
  Q_D(ctkDICOMAppWidget);
  d->DICOMModel.reset();
  emit this->queryRetrieveFinished();
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onRemoveAction()
{
  Q_D(ctkDICOMAppWidget);

  //d->QueryRetrieveWidget->show();
  // d->QueryRetrieveWidget->raise();
  std::cout << "on remove" << std::endl;
  QModelIndexList selection = d->TreeView->selectionModel()->selectedIndexes();
  std::cout << selection.size() << std::endl;
  QModelIndex index;
  foreach(index,selection)
  {
    QModelIndex index0 = index.sibling(index.row(), 0);
    if ( d->DICOMModel.data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::SeriesType))
    {
      QString seriesUID = d->DICOMModel.data(index0,ctkDICOMModel::UIDRole).toString();
      d->DICOMDatabase->removeSeries(seriesUID);
    } 
    else if ( d->DICOMModel.data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::StudyType))
    {
      QString studyUID = d->DICOMModel.data(index0,ctkDICOMModel::UIDRole).toString();
      d->DICOMDatabase->removeStudy(studyUID);
    }
    else if ( d->DICOMModel.data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::PatientType))
    {
      QString patientUID = d->DICOMModel.data(index0,ctkDICOMModel::UIDRole).toString();
      d->DICOMDatabase->removePatient(patientUID);
    }
  }
  d->DICOMModel.reset();
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::suspendModel()
{
  Q_D(ctkDICOMAppWidget);

  d->DICOMModel.setDatabase(d->EmptyDatabase);
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::resumeModel()
{
  Q_D(ctkDICOMAppWidget);

  d->DICOMModel.setDatabase(d->DICOMModelDatabase);
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::resetModel()
{
  Q_D(ctkDICOMAppWidget);

  d->DICOMModel.reset();
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onProgress(int progress)
{
  Q_UNUSED(progress);
  QApplication::processEvents();
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onThumbnailSelected(const ctkThumbnailLabel& widget)
{
    Q_D(ctkDICOMAppWidget);

  QModelIndex index = widget.property("sourceIndex").value<QPersistentModelIndex>();
  if(index.isValid())
    {
    d->ImagePreview->onModelSelected(index);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onThumbnailDoubleClicked(const ctkThumbnailLabel& widget)
{
    Q_D(ctkDICOMAppWidget);

    QModelIndex index = widget.property("sourceIndex").value<QPersistentModelIndex>();

    if(!index.isValid())
      {
      return;
      }

    ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(index.model()));
    QModelIndex index0 = index.sibling(index.row(), 0);

    if(model && (model->data(index0,ctkDICOMModel::TypeRole) != static_cast<int>(ctkDICOMModel::ImageType)))
      {
        this->onModelSelected(index0);
        d->TreeView->setCurrentIndex(index0);
        d->ThumbnailsWidget->onModelSelected(index0);
        d->ImagePreview->onModelSelected(index0);
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onImportDirectory(QString directory)
{
  Q_D(ctkDICOMAppWidget);
  if (QDir(directory).exists())
    {
    QCheckBox* copyOnImport = qobject_cast<QCheckBox*>(d->ImportDialog->bottomWidget());
    QString targetDirectory;
    if (copyOnImport->checkState() == Qt::Checked)
      {
      targetDirectory = d->DICOMDatabase->databaseDirectory();
      }
    QProgressDialog* progress = new QProgressDialog("DICOM Import", "Cancel", 0, 100, this,
                           Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
    // We don't want the progress dialog to resize itself, so we bypass the label
    // by creating our own
    QLabel* progressLabel = new QLabel(tr("Initialization..."));
    progress->setLabel(progressLabel);
#ifdef Q_WS_MAC
    // BUG: avoid deadlock of dialogs on mac
    progress->setWindowModality(Qt::NonModal);
#else
    progress->setWindowModality(Qt::ApplicationModal);
#endif
    progress->setMinimumDuration(0);
    progress->setValue(0);
    progress->show();

    connect(progress, SIGNAL(canceled()), d->DICOMIndexer.data(), SLOT(cancel()));
    connect(d->DICOMIndexer.data(), SIGNAL(indexingFilePath(QString)),
            progressLabel, SLOT(setText(QString)));
    connect(d->DICOMIndexer.data(), SIGNAL(progress(int)),
            progress, SLOT(setValue(int)));
    connect(d->DICOMIndexer.data(), SIGNAL(progress(int)),
            this, SLOT(onProgress(int)));

    connect(d->DICOMIndexer.data(), SIGNAL(indexingComplete()),
            progress, SLOT(close()));
    connect(d->DICOMIndexer.data(), SIGNAL(indexingComplete()),
            &d->DICOMModel, SLOT(reset()));

    d->DICOMIndexer->addDirectory(*d->DICOMDatabase,directory,targetDirectory);
  }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onModelSelected(const QModelIndex &index){
Q_D(ctkDICOMAppWidget);

    ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(index.model()));

    if(model)
      {
        QModelIndex index0 = index.sibling(index.row(), 0);

        if ( model->data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::PatientType) )
          {
          d->NextImageButton->show();
          d->PrevImageButton->show();
          d->NextSeriesButton->show();
          d->PrevSeriesButton->show();
          d->NextStudyButton->show();
          d->PrevStudyButton->show();
          }
        else if ( model->data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::StudyType) )
          {
          d->NextImageButton->show();
          d->PrevImageButton->show();
          d->NextSeriesButton->show();
          d->PrevSeriesButton->show();
          d->NextStudyButton->hide();
          d->PrevStudyButton->hide();
          }
        else if ( model->data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::SeriesType) )
          {
          d->NextImageButton->show();
          d->PrevImageButton->show();
          d->NextSeriesButton->hide();
          d->PrevSeriesButton->hide();
          d->NextStudyButton->hide();
          d->PrevStudyButton->hide();
          }
        else
          {
          d->NextImageButton->hide();
          d->PrevImageButton->hide();
          d->NextSeriesButton->hide();
          d->PrevSeriesButton->hide();
          d->NextStudyButton->hide();
          d->PrevStudyButton->hide();
          }
        d->ActionRemove->setEnabled(
            model->data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::SeriesType) ||
            model->data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::StudyType) ||
            model->data(index0,ctkDICOMModel::TypeRole) == static_cast<int>(ctkDICOMModel::PatientType) );
        }

      else
        {
        d->NextImageButton->hide();
        d->PrevImageButton->hide();
        d->NextSeriesButton->hide();
        d->PrevSeriesButton->hide();
        d->NextStudyButton->hide();
        d->PrevStudyButton->hide();
        d->ActionRemove->setEnabled(false);
        }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onNextImage(){
    Q_D(ctkDICOMAppWidget);

    QModelIndex currentIndex = d->ImagePreview->currentImageIndex();

    if(currentIndex.isValid())
      {
      ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(currentIndex.model()));

      if(model)
        {
        QModelIndex seriesIndex = currentIndex.parent();

        int imageCount = model->rowCount(seriesIndex);
        int imageID = currentIndex.row();

        imageID = (imageID+1)%imageCount;

        int max = d->PlaySlider->maximum();
        if(imageID > 0 && imageID < max)
          {
            d->PlaySlider->setValue(imageID);
          }

        QModelIndex nextIndex = currentIndex.sibling(imageID, 0);

        d->ImagePreview->onModelSelected(nextIndex);
        d->ThumbnailsWidget->selectThumbnailFromIndex(nextIndex);
        }
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onPreviousImage(){
    Q_D(ctkDICOMAppWidget);

    QModelIndex currentIndex = d->ImagePreview->currentImageIndex();

    if(currentIndex.isValid())
      {
      ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(currentIndex.model()));

      if(model)
        {
        QModelIndex seriesIndex = currentIndex.parent();

        int imageCount = model->rowCount(seriesIndex);
        int imageID = currentIndex.row();

        imageID--;
        if(imageID < 0) imageID += imageCount;

        int max = d->PlaySlider->maximum();
        if(imageID > 0 && imageID < max)
          {
            d->PlaySlider->setValue(imageID);
          }

        QModelIndex prevIndex = currentIndex.sibling(imageID, 0);

        d->ImagePreview->onModelSelected(prevIndex);
        d->ThumbnailsWidget->selectThumbnailFromIndex(prevIndex);
        }
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onNextSeries(){
    Q_D(ctkDICOMAppWidget);

    QModelIndex currentIndex = d->ImagePreview->currentImageIndex();

    if(currentIndex.isValid())
      {
      ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(currentIndex.model()));

      if(model)
        {
        QModelIndex seriesIndex = currentIndex.parent();
        QModelIndex studyIndex = seriesIndex.parent();

        int seriesCount = model->rowCount(studyIndex);
        int seriesID = seriesIndex.row();

        seriesID = (seriesID + 1)%seriesCount;

        QModelIndex nextIndex = seriesIndex.sibling(seriesID, 0);

        d->ImagePreview->onModelSelected(nextIndex);
        d->ThumbnailsWidget->selectThumbnailFromIndex(nextIndex);
        }
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onPreviousSeries(){
    Q_D(ctkDICOMAppWidget);

    QModelIndex currentIndex = d->ImagePreview->currentImageIndex();

    if(currentIndex.isValid())
      {
      ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(currentIndex.model()));

      if(model)
        {
        QModelIndex seriesIndex = currentIndex.parent();
        QModelIndex studyIndex = seriesIndex.parent();

        int seriesCount = model->rowCount(studyIndex);
        int seriesID = seriesIndex.row();

        seriesID--;
        if(seriesID < 0) seriesID += seriesCount;

        QModelIndex prevIndex = seriesIndex.sibling(seriesID, 0);

        d->ImagePreview->onModelSelected(prevIndex);
        d->ThumbnailsWidget->selectThumbnailFromIndex(prevIndex);
        }
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onNextStudy(){
    Q_D(ctkDICOMAppWidget);

    QModelIndex currentIndex = d->ImagePreview->currentImageIndex();

    if(currentIndex.isValid())
      {
      ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(currentIndex.model()));

      if(model)
        {
        QModelIndex seriesIndex = currentIndex.parent();
        QModelIndex studyIndex = seriesIndex.parent();
        QModelIndex patientIndex = studyIndex.parent();

        int studyCount = model->rowCount(patientIndex);
        int studyID = studyIndex.row();

        studyID = (studyID + 1)%studyCount;

        QModelIndex nextIndex = studyIndex.sibling(studyID, 0);

        d->ImagePreview->onModelSelected(nextIndex);
        d->ThumbnailsWidget->selectThumbnailFromIndex(nextIndex);
        }
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onPreviousStudy(){
    Q_D(ctkDICOMAppWidget);

    QModelIndex currentIndex = d->ImagePreview->currentImageIndex();

    if(currentIndex.isValid())
      {
      ctkDICOMModel* model = const_cast<ctkDICOMModel*>(qobject_cast<const ctkDICOMModel*>(currentIndex.model()));

      if(model)
        {
        QModelIndex seriesIndex = currentIndex.parent();
        QModelIndex studyIndex = seriesIndex.parent();
        QModelIndex patientIndex = studyIndex.parent();

        int studyCount = model->rowCount(patientIndex);
        int studyID = studyIndex.row();

        studyID--;
        if(studyID < 0) studyID += studyCount;

        QModelIndex prevIndex = studyIndex.sibling(studyID, 0);

        d->ImagePreview->onModelSelected(prevIndex);
        d->ThumbnailsWidget->selectThumbnailFromIndex(prevIndex);
        }
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onTreeCollapsed(const QModelIndex &index){
    Q_UNUSED(index);
    Q_D(ctkDICOMAppWidget);
    d->TreeView->resizeColumnToContents(0);
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onTreeExpanded(const QModelIndex &index){
    Q_UNUSED(index);
    Q_D(ctkDICOMAppWidget);
    d->TreeView->resizeColumnToContents(0);
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onAutoPlayCheckboxStateChanged(int state){
    Q_D(ctkDICOMAppWidget);

    if(state == 0) //OFF
      {
      disconnect(d->AutoPlayTimer, SIGNAL(timeout()), this, SLOT(onAutoPlayTimer()));
      d->AutoPlayTimer->deleteLater();
      }
    else if(state == 2) //ON
      {
      d->AutoPlayTimer = new QTimer(this);
      connect(d->AutoPlayTimer, SIGNAL(timeout()), this, SLOT(onAutoPlayTimer()));
      d->AutoPlayTimer->start(50);
      }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onAutoPlayTimer(){
    this->onNextImage();
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onThumbnailWidthSliderValueChanged(int val){
  Q_D(ctkDICOMAppWidget);
  d->ThumbnailsWidget->setThumbnailSize(QSize(val, val));
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onSearchParameterChanged(){
  Q_D(ctkDICOMAppWidget);
  d->DICOMModel.setDatabase(d->DICOMDatabase->database(), d->SearchOption->parameters());

  this->onModelSelected(d->DICOMModel.index(0,0));
  d->ThumbnailsWidget->clearThumbnails();
  d->ThumbnailsWidget->onModelSelected(d->DICOMModel.index(0,0));
  d->ImagePreview->clearImages();
  d->ImagePreview->onModelSelected(d->DICOMModel.index(0,0));
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onImagePreviewDisplayed(int imageID, int count){
  Q_D(ctkDICOMAppWidget);

  d->PlaySlider->setMinimum(0);
  d->PlaySlider->setMaximum(count-1);
  d->PlaySlider->setValue(imageID);
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onSearchPopUpButtonClicked(){
  Q_D(ctkDICOMAppWidget);

  if(d->SearchDockWidget->isFloating())
    {
    d->SearchDockWidget->hide();
    d->SearchDockWidget->setFloating(false);
    }
  else
    {
    d->SearchDockWidget->setFloating(true);
    d->SearchDockWidget->adjustSize();
    d->SearchDockWidget->show();
    }
}

//----------------------------------------------------------------------------
void ctkDICOMAppWidget::onSearchWidgetTopLevelChanged(bool topLevel){
  Q_D(ctkDICOMAppWidget);

  if(topLevel)
    {
    d->SearchDockWidget->show();
    }
  else
    {
    d->SearchDockWidget->hide();
    }
}
