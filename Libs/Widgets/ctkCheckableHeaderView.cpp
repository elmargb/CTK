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
/*=========================================================================

   Program: ParaView
   Module:    $RCSfile: pqCheckableHeaderView.cxx,v $

   Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

// Qt includes
#include <QAbstractItemModel>
#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

// CTK includes
#include "ctkCheckableHeaderView.h"
#include "ctkCheckBoxPixmaps.h"

//-----------------------------------------------------------------------------
class ctkCheckableHeaderViewPrivate
{
  Q_DECLARE_PUBLIC(ctkCheckableHeaderView);
protected:
  ctkCheckableHeaderView* const q_ptr;
public:
  ctkCheckableHeaderViewPrivate(ctkCheckableHeaderView& object);
  ~ctkCheckableHeaderViewPrivate();

  void init();
  void setIndexCheckState(const QModelIndex& index, Qt::CheckState checkState);
  int indexDepth(const QModelIndex& modelIndex)const;
  void updateCheckState(const QModelIndex& modelIndex);
  void propagateCheckStateToChildren(const QModelIndex& modelIndex);

  int                 Pressed;
  ctkCheckBoxPixmaps* CheckBoxPixmaps;
  bool                HeaderIsUpdating;
  bool                ItemsAreUpdating;
  bool                PropagateToItems;
  int                 PropagateDepth;
};

//----------------------------------------------------------------------------
ctkCheckableHeaderViewPrivate::ctkCheckableHeaderViewPrivate(ctkCheckableHeaderView& object)
  :q_ptr(&object)
{
  this->HeaderIsUpdating = false;
  this->ItemsAreUpdating = false;
  this->CheckBoxPixmaps = 0;
  this->Pressed = -1;
  this->PropagateToItems = true;
  this->PropagateDepth = -1;
}

//-----------------------------------------------------------------------------
ctkCheckableHeaderViewPrivate::~ctkCheckableHeaderViewPrivate()
{
  if (this->CheckBoxPixmaps)
    {
    delete this->CheckBoxPixmaps;
    this->CheckBoxPixmaps = 0;
    }
}

//----------------------------------------------------------------------------
void ctkCheckableHeaderViewPrivate::init()
{
  Q_Q(ctkCheckableHeaderView);
  this->CheckBoxPixmaps = new ctkCheckBoxPixmaps(q);
}

//----------------------------------------------------------------------------
void ctkCheckableHeaderViewPrivate::setIndexCheckState(
  const QModelIndex& index, Qt::CheckState checkState)
{
  Q_Q(ctkCheckableHeaderView);
  int depth = this->indexDepth(index);
  if (depth > this->PropagateDepth && this->PropagateDepth != -1)
    {
    return;
    }
  QVariant indexCheckState = q->model()->data(index, Qt::CheckStateRole);
  bool checkable = false;
  int state = indexCheckState.toInt(&checkable);
  if (checkable && state == checkState)
    {// TODO: here if you don't want to overwrite the uncheckability of indexes
    return;
    }
  q->model()->setData(index, checkState, Qt::CheckStateRole);
  this->propagateCheckStateToChildren(index);
}

//-----------------------------------------------------------------------------
int ctkCheckableHeaderViewPrivate::indexDepth(const QModelIndex& modelIndex)const
{
  int depth = 0;
  QModelIndex parentIndex = modelIndex;
  while (parentIndex.isValid())
    {
    ++depth;
    parentIndex = parentIndex.parent();
    }
  return depth;
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderViewPrivate
::updateCheckState(const QModelIndex& modelIndex)
{
  Q_Q(ctkCheckableHeaderView);
  QVariant indexCheckState = modelIndex != q->rootIndex() ?
    q->model()->data(modelIndex, Qt::CheckStateRole):
    q->model()->headerData(0, q->orientation(), Qt::CheckStateRole);
  bool checkable = false;
  int oldCheckState = indexCheckState.toInt(&checkable);
  if (!checkable)
    {
    return;
    }

  Qt::CheckState newCheckState = Qt::PartiallyChecked;
  bool firstCheckableChild = true;
  const int rowCount = q->orientation() == Qt::Horizontal ?
    q->model()->rowCount(modelIndex) : 1;
  const int columnCount = q->orientation() == Qt::Vertical ?
    q->model()->columnCount(modelIndex) : 1;
  for (int r = 0; r < rowCount; ++r)
    {
    for (int c = 0; c < columnCount; ++c)
      {
      QModelIndex child = q->model()->index(r, c, modelIndex);
      QVariant childCheckState = q->model()->data(child, Qt::CheckStateRole);
      int childState = childCheckState.toInt(&checkable);
      if (!checkable)
        {
        continue;
        }
      if (firstCheckableChild)
        {
        newCheckState = static_cast<Qt::CheckState>(childState);
        firstCheckableChild = false;
        }
      if (newCheckState != childState)
        {
        newCheckState = Qt::PartiallyChecked;
        }
      if (newCheckState == Qt::PartiallyChecked)
        {
        break;
        }
      }
    if (!firstCheckableChild && newCheckState == Qt::PartiallyChecked)
      {
      break;
      } 
    }
  if (oldCheckState == newCheckState)
    {
    return;
    }
  if (modelIndex != q->rootIndex())
    {
    q->model()->setData(modelIndex, newCheckState, Qt::CheckStateRole);
    this->updateCheckState(modelIndex.parent());
    }
  else
    {
    q->model()->setHeaderData(0, q->orientation(), newCheckState, Qt::CheckStateRole);
    }
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderViewPrivate
::propagateCheckStateToChildren(const QModelIndex& modelIndex)
{
  Q_Q(ctkCheckableHeaderView);
  int indexDepth = this->indexDepth(modelIndex);
  if (indexDepth > this->PropagateDepth && this->PropagateDepth != -1)
    {
    return;
    }

  QVariant indexCheckState = (modelIndex != q->rootIndex() ?
    q->model()->data(modelIndex, Qt::CheckStateRole):
    q->model()->headerData(0, q->orientation(), Qt::CheckStateRole));
  bool checkable = false;
  Qt::CheckState checkState =
    static_cast<Qt::CheckState>(indexCheckState.toInt(&checkable));
  if (!checkable || checkState == Qt::PartiallyChecked)
    {
    return;
    }
  const int rowCount = q->orientation() == Qt::Horizontal ?
    q->model()->rowCount(modelIndex) : 1;
  const int columnCount = q->orientation() == Qt::Vertical ?
    q->model()->columnCount(modelIndex) : 1;
  for (int r = 0; r < rowCount; ++r)
    {
    for (int c = 0; c < columnCount; ++c)
      {
      QModelIndex child = q->model()->index(r, c, modelIndex);
      this->setIndexCheckState(child, checkState);
      }
    }
}

//----------------------------------------------------------------------------
ctkCheckableHeaderView::ctkCheckableHeaderView(
  Qt::Orientation orient, QWidget *widgetParent)
  : QHeaderView(orient, widgetParent)
  , d_ptr(new ctkCheckableHeaderViewPrivate(*this))
{
  Q_D(ctkCheckableHeaderView);
  d->init();
  if(widgetParent)
    {
    // Listen for focus change events.
    widgetParent->installEventFilter(this);
    }
}

//-----------------------------------------------------------------------------
ctkCheckableHeaderView::~ctkCheckableHeaderView()
{
}

//-----------------------------------------------------------------------------
bool ctkCheckableHeaderView::eventFilter(QObject *, QEvent *e)
{
  if(e->type() != QEvent::FocusIn && 
     e->type() != QEvent::FocusOut)
    {
    return false;
    }
  this->updateHeaders();
  return false;
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::setModel(QAbstractItemModel *newModel)
{
  Q_D(ctkCheckableHeaderView);
  QAbstractItemModel *current = this->model();
  if (current == newModel)
    {
    return;
    }
  if(current)
    {
    this->disconnect(
      current, SIGNAL(headerDataChanged(Qt::Orientation, int, int)),
      this, SLOT(updateHeaderData(Qt::Orientation, int, int)));
    this->disconnect(
      current, SIGNAL(modelReset()),
      this, SLOT(updateHeaders()));
    this->disconnect(
      current, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
      this, SLOT(updateHeadersFromItems(const QModelIndex&, const QModelIndex&)));
    this->disconnect(
      current, SIGNAL(columnsInserted(const QModelIndex &, int, int)), 
      this, SLOT(insertHeaderSection(const QModelIndex &, int, int)));
    this->disconnect(
      current, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
      this, SLOT(insertHeaderSection(const QModelIndex &, int, int)));
    }

  this->QHeaderView::setModel(newModel);
  if(newModel)
    {
    this->connect(
      newModel, SIGNAL(headerDataChanged(Qt::Orientation, int, int)),
      this, SLOT(updateHeaderData(Qt::Orientation, int, int)));
    this->connect(
      newModel, SIGNAL(modelReset()),
      this, SLOT(updateHeaders()));
    if (d->PropagateToItems)
      {
      this->connect(
        newModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
        this, SLOT(updateHeadersFromItems(const QModelIndex&, const QModelIndex&)));
      }
    if(this->orientation() == Qt::Horizontal)
      {
      this->connect(
        newModel, SIGNAL(columnsInserted(const QModelIndex &, int, int)),
        this, SLOT(insertHeaderSection(const QModelIndex &, int, int)));
      }
    else
      {
      this->connect(
        newModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
        this, SLOT(insertHeaderSection(const QModelIndex &, int, int)));
      }
    }

  // Determine which sections are clickable and setup the icons.
  if (d->PropagateToItems)
    {
    this->updateHeadersFromItems();
    }
  else
    {
    this->updateHeaders();
    }
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::setRootIndex(const QModelIndex &index)
{
  Q_D(ctkCheckableHeaderView);
  this->QHeaderView::setRootIndex(index);
  if (d->PropagateToItems)
    {
    this->updateHeadersFromItems();
    }
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::setPropagateToItems(bool propagate)
{
  Q_D(ctkCheckableHeaderView);
  if (d->PropagateToItems == propagate)
    {
    return;
    }
  d->PropagateToItems = propagate;
  if (!this->model())
    {
    return;
    }
  if (propagate)
    {
    this->connect(
      this->model(), SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
      this, SLOT(updateHeadersFromItems(const QModelIndex&, const QModelIndex&)));
    this->updateHeadersFromItems();
    }
  else
    {
    this->disconnect(
      this->model(), SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
      this, SLOT(updateHeadersFromItems(const QModelIndex&, const QModelIndex&)));
    }
}

//-----------------------------------------------------------------------------
bool ctkCheckableHeaderView::propagateToItems()const
{
  Q_D(const ctkCheckableHeaderView);
  return d->PropagateToItems;
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::setPropagateDepth(int depth)
{
  Q_D(ctkCheckableHeaderView);
  d->PropagateDepth = depth;
  //TODO: rescan the model
}

//-----------------------------------------------------------------------------
int ctkCheckableHeaderView::propagateDepth()const
{
  Q_D(const ctkCheckableHeaderView);
  return d->PropagateDepth;
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::toggleCheckState(int section)
{
  // If the section is checkable, toggle the check state.
  if(!this->isCheckable(section))
    {    
    return;
    }
  // I've no strong feeling to turn the state checked or unchecked when the 
  // state is PartiallyChecked.
  this->setCheckState(section, this->checkState(section) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::setCheckState(int section, Qt::CheckState checkState)
{
  Q_D(ctkCheckableHeaderView);
  QAbstractItemModel *current = this->model();
  if(current == 0)
    {    
    return;
    }
  current->setHeaderData(section, this->orientation(),
                         checkState, Qt::CheckStateRole);
  if (d->PropagateToItems)
    {
    d->ItemsAreUpdating = true;
    d->propagateCheckStateToChildren(this->rootIndex());
    d->ItemsAreUpdating = false;
    }
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::updateHeaderData(Qt::Orientation orient,
                                              int firstSection,
                                              int lastSection)
{
  if(orient != this->orientation())
    {
    return;
    }
  this->updateHeaders(firstSection, lastSection);
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::updateHeaders(int firstSection, int lastSection)
{
  Q_D(ctkCheckableHeaderView);
  QAbstractItemModel *current = this->model();
  if(d->HeaderIsUpdating || !current)
    {
    return;
    }
  d->HeaderIsUpdating = true;

  firstSection = qBound(0, firstSection, this->count() -1);
  lastSection = qBound(0, lastSection, this->count() -1);

  bool active = true;
  if(this->parentWidget())
    {
    active = this->parentWidget()->hasFocus();
    }
  for(int i = firstSection; i <= lastSection; i++)
    {
    QVariant decoration;
    Qt::CheckState checkState;
    if (this->checkState(i, checkState))
      {
      decoration = d->CheckBoxPixmaps->pixmap(checkState, active);
      }
    current->setHeaderData(i, this->orientation(), decoration,
                           Qt::DecorationRole);
    }
  d->HeaderIsUpdating = false;
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::updateHeadersFromItems()
{
  Q_D(ctkCheckableHeaderView);
  QAbstractItemModel *currentModel = this->model();
  if (!currentModel)
    {
    return;
    }
  d->updateCheckState(QModelIndex());
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::updateHeadersFromItems(const QModelIndex & topLeft,
                                                    const QModelIndex & bottomRight)
{
  Q_UNUSED(bottomRight);
  Q_D(ctkCheckableHeaderView);
  if(d->ItemsAreUpdating || !d->PropagateToItems)
    {
    return;
    }
  bool checkable = false;
  QVariant topLeftCheckState = this->model()->data(topLeft, Qt::CheckStateRole);
  topLeftCheckState.toInt(&checkable);
  if (!checkable)
    {
    return;
    }
  d->ItemsAreUpdating = true;

  d->propagateCheckStateToChildren(topLeft);
  d->updateCheckState(topLeft.parent());

  d->ItemsAreUpdating = false;
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::insertHeaderSection(const QModelIndex &parentIndex,
    int first, int last)
{
  if (this->rootIndex() != parentIndex)
    {
    return;
    }
  this->updateHeaders(first, last);
}

//-----------------------------------------------------------------------------
bool ctkCheckableHeaderView::isCheckable(int section)const
{
  return !this->model()->headerData(section, this->orientation(), Qt::CheckStateRole).isNull();
}

//-----------------------------------------------------------------------------
Qt::CheckState ctkCheckableHeaderView::checkState(int section)const
{
  return static_cast<Qt::CheckState>(
    this->model()->headerData(section, this->orientation(), Qt::CheckStateRole).toInt());
}

//-----------------------------------------------------------------------------
bool ctkCheckableHeaderView::checkState(int section, Qt::CheckState& checkState)const
{
  bool checkable = false;
  checkState = static_cast<Qt::CheckState>(
    this->model()->headerData(section, this->orientation(), Qt::CheckStateRole).toInt(&checkable));
  return checkable;
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::mousePressEvent(QMouseEvent *e)
{
  Q_D(ctkCheckableHeaderView);
  if (e->button() != Qt::LeftButton || 
      d->Pressed >= 0)
    {
    d->Pressed = -1;
    this->QHeaderView::mousePressEvent(e);
    return;
    }
  d->Pressed = -1;
  //check if the check box is pressed
  int pos = this->orientation() == Qt::Horizontal ? e->x() : e->y();
  int section = this->logicalIndexAt(pos);
  if (this->isCheckable(section) &&
      this->isPointInCheckBox(section, e->pos()))
    {
    d->Pressed = section;
    }
  this->QHeaderView::mousePressEvent(e);
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::mouseReleaseEvent(QMouseEvent *e)
{
  Q_D(ctkCheckableHeaderView);
  if (e->button() != Qt::LeftButton || 
      d->Pressed < 0)
    {
    d->Pressed = -1;
    this->QHeaderView::mouseReleaseEvent(e);
    return;
    }
  //check if the check box is pressed
  int pos = this->orientation() == Qt::Horizontal ? e->x() : e->y();
  int section = this->logicalIndexAt(pos);
  if (section == d->Pressed && 
      this->isPointInCheckBox(section, e->pos()))
    {
    d->Pressed = -1;
    this->toggleCheckState(section);
    }
  this->QHeaderView::mousePressEvent(e);
}

//-----------------------------------------------------------------------------
bool ctkCheckableHeaderView::isPointInCheckBox(int section, QPoint pos)const
{
  QRect sectionRect = this->orientation() == Qt::Horizontal ? 
    QRect(this->sectionPosition(section), 0, 
          this->sectionSize(section), this->height()):
    QRect(0, this->sectionPosition(section), 
          this->width(), this->sectionSize(section));
  QStyleOptionHeader opt;
  this->initStyleOption(&opt);
  this->initStyleSectionOption(&opt, section, sectionRect);
  QRect headerLabelRect = this->style()->subElementRect(QStyle::SE_HeaderLabel, &opt, this);
  // from qcommonstyle.cpp:1541
  if (opt.icon.isNull()) 
    {
    return false;
    }
  QPixmap pixmap
    = opt.icon.pixmap(this->style()->pixelMetric(QStyle::PM_SmallIconSize), 
                      (opt.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled);
  QRect aligned = this->style()->alignedRect(opt.direction, QFlag(opt.iconAlignment), 
                              pixmap.size(), headerLabelRect);
  QRect inter = aligned.intersected(headerLabelRect);
  return inter.contains(pos);
}

//-----------------------------------------------------------------------------
void ctkCheckableHeaderView::initStyleSectionOption(QStyleOptionHeader *option, int section, QRect rect)const
{
  // from qheaderview.cpp:paintsection
  QStyle::State state = QStyle::State_None;
  if (this->isEnabled())
    {
    state |= QStyle::State_Enabled;
    }
  if (this->window()->isActiveWindow())
    {
    state |= QStyle::State_Active;
    }
  if (this->isSortIndicatorShown() && 
      this->sortIndicatorSection() == section)
    {
    option->sortIndicator = (this->sortIndicatorOrder() == Qt::AscendingOrder)
      ? QStyleOptionHeader::SortDown : QStyleOptionHeader::SortUp;
    }

  // setup the style option structure
  QVariant textAlignment = 
    this->model()->headerData(section, this->orientation(),
                              Qt::TextAlignmentRole);
  option->rect = rect;
  option->section = section;
  option->state |= state;
  option->textAlignment = Qt::Alignment(textAlignment.isValid()
                                        ? Qt::Alignment(textAlignment.toInt())
                                        : this->defaultAlignment());
  
  option->iconAlignment = Qt::AlignVCenter;
  option->text = this->model()->headerData(section, this->orientation(),
                                  Qt::DisplayRole).toString();
  if (this->textElideMode() != Qt::ElideNone)
    {
    option->text = option->fontMetrics.elidedText(option->text, this->textElideMode() , rect.width() - 4);
    }

  QVariant variant = this->model()->headerData(section, this->orientation(),
                                          Qt::DecorationRole);
  option->icon = qvariant_cast<QIcon>(variant);
  if (option->icon.isNull())
    {
    option->icon = qvariant_cast<QPixmap>(variant);
    }
  QVariant foregroundBrush = this->model()->headerData(section, this->orientation(),
                                                  Qt::ForegroundRole);
  if (qVariantCanConvert<QBrush>(foregroundBrush))
    {
    option->palette.setBrush(QPalette::ButtonText, qvariant_cast<QBrush>(foregroundBrush));
    }

  //QPointF oldBO = painter->brushOrigin();
  QVariant backgroundBrush = this->model()->headerData(section, this->orientation(),
                                                  Qt::BackgroundRole);
  if (qVariantCanConvert<QBrush>(backgroundBrush)) 
    {
    option->palette.setBrush(QPalette::Button, qvariant_cast<QBrush>(backgroundBrush));
    option->palette.setBrush(QPalette::Window, qvariant_cast<QBrush>(backgroundBrush));
    //painter->setBrushOrigin(option->rect.topLeft());
    }

  // the section position
  int visual = this->visualIndex(section);
  Q_ASSERT(visual != -1);
  if (this->count() == 1)
    {
    option->position = QStyleOptionHeader::OnlyOneSection;
    }
  else if (visual == 0)
    {
    option->position = QStyleOptionHeader::Beginning;
    }
  else if (visual == this->count() - 1)
    {
    option->position = QStyleOptionHeader::End;
    }
  else
    {
    option->position = QStyleOptionHeader::Middle;
    }
  option->orientation = this->orientation();
  /* the selected position
  bool previousSelected = d->isSectionSelected(this->logicalIndex(visual - 1));
  bool nextSelected =  d->isSectionSelected(this->logicalIndex(visual + 1));
  if (previousSelected && nextSelected)
    option->selectedPosition = QStyleOptionHeader::NextAndPreviousAreSelected;
  else if (previousSelected)
    option->selectedPosition = QStyleOptionHeader::PreviousIsSelected;
  else if (nextSelected)
    option->selectedPosition = QStyleOptionHeader::NextIsSelected;
  else
    option->selectedPosition = QStyleOptionHeader::NotAdjacent;
  */
}
    
