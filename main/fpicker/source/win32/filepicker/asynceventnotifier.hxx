/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#ifndef _ASYNCEVENTNOTIFIER_HXX_
#define _ASYNCEVENTNOTIFIER_HXX_

//------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------

#include <osl/mutex.hxx>
#include <osl/conditn.hxx>
#include <cppuhelper/interfacecontainer.h>

#if defined _MSC_VER
#pragma warning(push, 1)
#endif
#include <windows.h>
#if defined _MSC_VER
#pragma warning(pop)
#endif

#include <list>
#include <utility>
#include "eventnotification.hxx"

//---------------------------------------------
//
//---------------------------------------------

class CAsyncEventNotifier
{
public:
    CAsyncEventNotifier(cppu::OBroadcastHelper& rBroadcastHelper);
    ~CAsyncEventNotifier();

	bool SAL_CALL startup(bool bCreateSuspended = true);
	void SAL_CALL shutdown();

    // notifications may be added the
    // the event queue but will only 
    // be notified to the clients after
    // resume was called
    void suspend();
    
    // resume notifying events
    void resume();
     
	// this class is responsible for the memory management of
	// the CEventNotification instance
    void SAL_CALL notifyEvent(CEventNotification* EventNotification);

    void SAL_CALL addListener   (const ::com::sun::star::uno::Type&                                           aType    ,
                                 const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface >& xListener);
    void SAL_CALL removeListener(const ::com::sun::star::uno::Type&                                           aType    ,
                                 const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface >& xListener);

private:
	size_t              SAL_CALL getEventListSize();
	void                SAL_CALL resetNotifyEvent();
	CEventNotification* SAL_CALL getNextEventRecord();
	void                SAL_CALL removeNextEventRecord();

    void SAL_CALL run( );

	static unsigned int WINAPI ThreadProc(LPVOID pParam);

private:    
    std::list<CEventNotification*>  m_EventList;
    HANDLE							m_hThread;
	bool							m_bRun;
	unsigned						m_ThreadId;    
    ::cppu::OBroadcastHelper&		m_rBroadcastHelper;		
    HANDLE                          m_hEvents[2];
    HANDLE&                         m_NotifyEvent;
    HANDLE&                         m_ResumeNotifying;	
    osl::Mutex						m_Mutex;    

// prevent copy and assignment
private:
    CAsyncEventNotifier( const CAsyncEventNotifier& );
    CAsyncEventNotifier& operator=( const CAsyncEventNotifier& );
};

#endif
