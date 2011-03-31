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

#ifndef __ctkAbstractLibraryFactory_tpp
#define __ctkAbstractLibraryFactory_tpp

// CTK includes
#include "ctkAbstractLibraryFactory.h"

//----------------------------------------------------------------------------
// ctkFactoryLibraryItem methods
/*
//----------------------------------------------------------------------------
template<typename BaseClassType>
ctkFactoryLibraryItem<BaseClassType>::ctkFactoryLibraryItem(const QString& _path)
  :ctkAbstractFactoryFileBasedItem<BaseClassType>(_path)
{
}*/

//----------------------------------------------------------------------------
template<typename BaseClassType>
bool ctkFactoryLibraryItem<BaseClassType>::load()
{
  this->Library.setFileName(this->path());
  bool loaded = this->Library.load();
  if (loaded)
    {
    if (!this->resolve())
      {
      return false;
      }
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
template<typename BaseClassType>
QString ctkFactoryLibraryItem<BaseClassType>::loadErrorString()const
{
  return this->Library.errorString();
}

//----------------------------------------------------------------------------
template<typename BaseClassType>
void ctkFactoryLibraryItem<BaseClassType>::setSymbols(const QStringList& symbols)
{ 
  this->Symbols = symbols; 
}

//-----------------------------------------------------------------------------
template<typename BaseClassType>
bool ctkFactoryLibraryItem<BaseClassType>::resolve()
{
  foreach(const QString& symbol, this->Symbols)
    {
    // Sanity checks
    if (symbol.isEmpty()) 
      { 
      continue; 
      }
      
    // Make sure the symbols haven't been registered
    if (this->ResolvedSymbols.contains(symbol))
      {
      if (this->verbose())
        {
        qWarning() << "Symbol '" << symbol << "' already resolved - Path:" << this->path();
        }
      continue;
      }

    void * resolvedSymbol = this->Library.resolve(symbol.toLatin1());
    if (!resolvedSymbol)
      {
      return false;
      }
    this->ResolvedSymbols[symbol] = resolvedSymbol;
    }
  return true;
}

//-----------------------------------------------------------------------------
template<typename BaseClassType>
void* ctkFactoryLibraryItem<BaseClassType>::symbolAddress(const QString& symbol)const
{
  ConstIterator iter = this->ResolvedSymbols.find(symbol);
  
  Q_ASSERT(iter != this->ResolvedSymbols.constEnd());
  if ( iter == this->ResolvedSymbols.constEnd())
    {
    return 0;
    }
  return iter.value();
}

//----------------------------------------------------------------------------
// ctkAbstractLibraryFactory methods

//-----------------------------------------------------------------------------
template<typename BaseClassType>
void ctkAbstractLibraryFactory<BaseClassType>::setSymbols(
  const QStringList& symbols)
{
  this->Symbols = symbols;
}

//-----------------------------------------------------------------------------
template<typename BaseClassType>
bool ctkAbstractLibraryFactory<BaseClassType>
::isValidFile(const QFileInfo& file)const
{
  return this->ctkAbstractFileBasedFactory<BaseClassType>::isValidFile(file) &&
    QLibrary::isLibrary(file.fileName());
}

//-----------------------------------------------------------------------------
template<typename BaseClassType>
void ctkAbstractLibraryFactory<BaseClassType>::
initItem(ctkAbstractFactoryItem<BaseClassType>* item)
{
  this->ctkAbstractFileBasedFactory<BaseClassType>::initItem(item);
  dynamic_cast<ctkFactoryLibraryItem<BaseClassType>*>(item)->setSymbols(this->Symbols);
}

#endif
