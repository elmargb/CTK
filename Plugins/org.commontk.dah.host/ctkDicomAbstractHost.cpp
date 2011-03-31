/*=============================================================================

  Library: CTK

  Copyright (c) German Cancer Research Center,
    Division of Medical and Biological Informatics

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=============================================================================*/

// CTK includes
#include "ctkDicomAppService.h"
#include "ctkDicomAbstractHost.h"
#include "ctkDicomHostServer.h"
#include "ctkDicomAppService.h"
#include "ctkDicomAppHostingTypesHelper.h"
#include <ctkDicomObjectLocatorCache.h>

class ctkDicomAbstractHostPrivate
{
public:

  ctkDicomAbstractHostPrivate(ctkDicomAbstractHost* hostInterface, int HostPort, int AppPort);
  ~ctkDicomAbstractHostPrivate();

  int HostPort;
  int AppPort;
  ctkDicomHostServer* Server;
  ctkDicomAppInterface* AppService;
  ctkDicomAppHosting::State AppState;
  ctkDicomObjectLocatorCache ObjectLocatorCache;
  // ctkDicomAppHosting::Status

};

//----------------------------------------------------------------------------
// ctkDicomAbstractHostPrivate methods

//----------------------------------------------------------------------------
ctkDicomAbstractHostPrivate::ctkDicomAbstractHostPrivate(
  ctkDicomAbstractHost* hostInterface, int hostPort, int appPort) : HostPort(hostPort), AppPort(appPort),AppState(ctkDicomAppHosting::EXIT)
{
  // start server
  if (this->HostPort == 0)
    {
    this->HostPort = 8080;
    }
  if (this->AppPort == 0)
    {
    this->AppPort = 8081;
    }

  this->Server = new ctkDicomHostServer(hostInterface,hostPort);
  this->AppService = new ctkDicomAppService(appPort, "/ApplicationInterface");
}

//----------------------------------------------------------------------------
ctkDicomAbstractHostPrivate::~ctkDicomAbstractHostPrivate()
{
  delete this->Server;
  this->Server = 0;
  //do not delete AppService, deleted somewhere else before?
  //delete  this->AppService;
  //this->AppService = 0;
}

//----------------------------------------------------------------------------
// ctkDicomAbstractHost methods

//----------------------------------------------------------------------------
ctkDicomAbstractHost::ctkDicomAbstractHost(int hostPort, int appPort) :
  d_ptr(new ctkDicomAbstractHostPrivate(this, hostPort, appPort))
{

}

//----------------------------------------------------------------------------
ctkDicomAbstractHost::~ctkDicomAbstractHost()
{
}

//----------------------------------------------------------------------------
int ctkDicomAbstractHost::getHostPort() const
{
  Q_D(const ctkDicomAbstractHost);
  return d->HostPort;
}

//----------------------------------------------------------------------------
int ctkDicomAbstractHost::getAppPort() const
{
  Q_D(const ctkDicomAbstractHost);
  return d->AppPort;
}

//----------------------------------------------------------------------------
ctkDicomAppInterface* ctkDicomAbstractHost::getDicomAppService() const
{
  Q_D(const ctkDicomAbstractHost);
  return d->AppService;
}

//----------------------------------------------------------------------------
QList<ctkDicomAppHosting::ObjectLocator> ctkDicomAbstractHost::getData(
  const QList<QUuid>& objectUUIDs,
  const QList<QString>& acceptableTransferSyntaxUIDs,
  bool includeBulkData)
{
  Q_UNUSED(acceptableTransferSyntaxUIDs);
  Q_UNUSED(includeBulkData);
  return this->objectLocatorCache()->getData(objectUUIDs);
}

//----------------------------------------------------------------------------
ctkDicomObjectLocatorCache* ctkDicomAbstractHost::objectLocatorCache()const
{
  Q_D(const ctkDicomAbstractHost);
  return const_cast<ctkDicomObjectLocatorCache*>(&d->ObjectLocatorCache);
}

//----------------------------------------------------------------------------
bool ctkDicomAbstractHost::publishData(const ctkDicomAppHosting::AvailableData& availableData, bool lastData)
{
  if (!this->objectLocatorCache()->isCached(availableData))
    {
    return false;
    }
  bool success = this->getDicomAppService()->notifyDataAvailable(availableData, lastData);
  if(!success)
    {
    return false;
    }
  return true;
}

//----------------------------------------------------------------------------
void ctkDicomAbstractHost::notifyStateChanged(ctkDicomAppHosting::State newState)
{
  qDebug()<< "new state notification received:"<< static_cast<int>(newState);
  qDebug()<< "new state notification received:"<< ctkDicomSoapState::toStringValue(newState);


  switch (newState){
  case ctkDicomAppHosting::IDLE:
    if (d_ptr->AppState == ctkDicomAppHosting::COMPLETED)
    {
      d_ptr->AppState = ctkDicomAppHosting::IDLE;
      releaseAvailableResources();
    }
    else if(d_ptr->AppState == ctkDicomAppHosting::EXIT
            || d_ptr->AppState == ctkDicomAppHosting::IDLE
            || d_ptr->AppState == ctkDicomAppHosting::CANCELED)
    {
      d_ptr->AppState = ctkDicomAppHosting::IDLE;
      emit appReady();
    }
    else{
      qDebug() << "Wrong transition from" << static_cast<int> (d_ptr->AppState)
                  << "to:" << static_cast<int>(newState);
    }
    break;

  case ctkDicomAppHosting::INPROGRESS:
    if (d_ptr->AppState == ctkDicomAppHosting::IDLE)
    {
      emit startProgress();
    }
    else if(d_ptr->AppState == ctkDicomAppHosting::SUSPENDED)
    {
      //shouldn't be necessary, but can be useful for feedback
      emit resumed();
    }
    else
    {
      qDebug() << "Wrong transition from" << static_cast<int>(d_ptr->AppState)
                  << "to:" << static_cast<int>(newState);
    }
    break;

  case ctkDicomAppHosting::COMPLETED:
    emit completed();
    break;

  case ctkDicomAppHosting::SUSPENDED:
    //shouldn't be necessary, but can be useful for feedback
    emit suspended();
    break;
  case ctkDicomAppHosting::CANCELED:
    //the app is in the process of canceling.
    //perhaps filtering for answers to a cancel commands and a cancel because of an error in the client
    emit canceled();
    break;

  case ctkDicomAppHosting::EXIT:
    //check if current state is IDLE
    emit exited();
    break;

  default:
    //should never happen
    qDebug() << "unexisting state Code, do nothing";
  }

  d_ptr->AppState = newState;
  emit stateChangedReceived(newState);
}

ctkDicomAppHosting::State ctkDicomAbstractHost::getApplicationState()const
{
  return d_ptr->AppState;
}
