//****************************************************************************
//
// Copyright (c) 2003-2009 Broadcom Corporation
//
// This program is the proprietary software of Broadcom Corporation and/or
// its licensors, and may only be used, duplicated, modified or distributed
// pursuant to the terms and conditions of a separate, written license
// agreement executed between you and Broadcom (an "Authorized License").
// Except as set forth in an Authorized License, Broadcom grants no license
// (express or implied), right to use, or waiver of any kind with respect to
// the Software, and Broadcom expressly reserves all rights in and to the
// Software and all intellectual property rights therein.  IF YOU HAVE NO
// AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY,
// AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE
// SOFTWARE.  
//
// Except as expressly set forth in the Authorized License,
//
// 1.     This program, including its structure, sequence and organization,
// constitutes the valuable trade secrets of Broadcom, and you shall use all
// reasonable efforts to protect the confidentiality thereof, and to use this
// information only in connection with your use of Broadcom integrated circuit
// products.
//
// 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
// "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS
// OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
// RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL
// IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR
// A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
// ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
// THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
//
// 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM
// OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
// INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY
// RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
// HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN
// EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1,
// WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY
// FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
//
//****************************************************************************
//  $Id$
//
//  Filename:       EpsCmDocsisApplication.h
//  Author:         John McQueen
//  Creation Date:  November 21, 2003
//
//****************************************************************************

#ifndef EpsCmDocsisApplication_H
#define EpsCmDocsisApplication_H

//********************** Include Files ***************************************

// My base class.
#include "BfcApplication.h"

#include "MessageLog.h"
#include "CableHomeDefines.h"
#include "HalIf.h"
#include "HalIfFactory.h"
#include "CableHomeCtrlThreadCmDocsisStatusClientACT.h"
#include "CableHomeCommonApplication.h"

//********************** Global Types ****************************************

//********************** Global Constants ************************************

//********************** Global Variable************************************

//********************** Forward Declarations ********************************

class BcmBaseIpHalIf;
class BcmCmDocsisCtlThread;


//********************** Class Declaration ***********************************


/** \ingroup CmDocsisSystem
*
*   This derived class handles CM DOCSIS-specific system initialization and
*   configuration.
*/
class BcmEpsCmDocsisApplication : public BcmCableHomeCommonApplication
{

public:

    /// Default Constructor.  Initializes the state of the object...
    ///
    /// Note that the constructor is called fairly early on in system startup.
    /// This places some limits on what can be done here, especially if there
    /// is any dependency on other system components.  We recommend that the
    /// work done here be limited to setting variables to default values, setting
    /// pointers to NULL, etc, and leaving the real work to the initialization
    /// API.
    ///
    BcmEpsCmDocsisApplication( bool dualForwarder );

    /// Destructor.  Frees up any memory/objects allocated, cleans up internal
    /// state.
    ///
    virtual ~BcmEpsCmDocsisApplication();

public:
  
    /// These methods are called at various points during system initialization.
    /// The order of the methods (from top to bottom) indicates the order in
    /// which they are called.  The comments for each method should indicate
    /// other conditions that may exist or that would keep the method from being
    /// called.
    
    /// Gives the application object a chance to add it's own version information
    /// to the version banner object.  It is useful to do this late so that the
    /// information can be configured based on run-time information.
    ///
    /// \param
    ///      pVersionBanner - pointer to the version banner object to which the
    ///                       version info should be added.
    ///
    /// \return
    ///      Nothing.  It doesn't matter whether or not there is a problem.
    ///
    virtual void AddVersionInfo(BcmVersionBanner *pVersionBanner);

    /// Called by the system object when it is time to shut everything down in
    /// preparation for exiting/rebooting the system.  This is a good place to
    /// stop threads, deregister things, etc.  You could wait to do this in your
    /// destructor, but by then other object may already be destroyed.  This
    /// method is called before anything is destroyed, so you know your pointers
    /// should still be valid.
    ///
    virtual void DeInitialize(void);


    
private:

    ///
    ///  This method will initialize and setup the coreCableHome/RG objects such as the
    ///  BcmCableHomeCtlThread, BcmCableHomeSnoopFactory, and BcmRgGatewayIpServiceManager.
    ///  
    ///  Any initialization done here MUST not have a dependency on SNMP being started because
    ///  the SNMP agent will be started after this call is executed.  
    ///
    /// \param
    ///     Nothing
    ///
    /// \return
    ///      Nothing.  It doesn't matter whether or not there is a problem.
    ///
    void CreateAndConfigureCableHome(void);


    /// Helper method to start RG/CableHome service that require that 
    /// SNMP be created and initialize before the service starts. An example
    /// of a service that may fall into this area of initialization are those
    /// that need to register persistent SNMP entry with the agent when being
    /// initialized.
    ///
    /// \param
    ///     Nothing
    ///
    /// \return
    ///      Nothing.  It doesn't matter whether or not there is a problem.
    ///
    void PostSnmpCableHomeConfiguration(void);
    

    /// This method will register the EPS CableHome Control Thread with the 
    /// Docsis Control thread events in order to provide synchronization between
    /// the two applications running.
    ///
    /// \param
    ///     pDocsisCtrlThread - pointer to the DOCSIS Control Thread
    ///
    /// \return
    ///      Nothing.  It doesn't matter whether or not there is a problem.
    ///
    void RegisterDocsisCtlThreadEvents(BcmCmDocsisCtlThread *pDocsisCtrlThread);


    /// This is implemented outside the library, in CableHomeAppFactoryCmMibs.cpp
    ///
    /// \param
    ///     Nothing
    ///
    /// \return
    ///      Nothing.  It doesn't matter whether or not there is a problem.
    ///
    void RegisterCmSideMibs(void);


public:

    ///
    ///  This method will shutdown the CableHome EPS ( reboot ) by 
    ///  basically calling the Cm reboot routine for an embedded PS.
    ///
    /// \param
    ///     Nothing
    ///
    /// \return
    ///      Nothing.  It doesn't matter whether or not there is a problem.
    ///
    void RebootApplication(void);

    
protected:

    /// The DOCSIS Control Thread; handles the DOCSIS state machine.
    BcmCmDocsisCtlThread *pfCmDocsisCtlThread;

    /// This ACT will handle Docsis events for the CableHome Control Thread
    BcmCableHomeCtrlThreadCmDocsisStatusClientACT *pfCableHomeCmDocsisStatusClientACT;

    
public:
                                                       
    /// Gets the DOCSIS Ctl Thread instance that is being used.
    ///
    /// \return
    ///      A pointer to the DOCSIS Ctl Thread instance.
    ///
    BcmCmDocsisCtlThread *CmDocsisCtlThread(void) const;

    /// Sets the DOCSIS Ctl Thread instance that is being used.
    ///
    /// \return
    ///      A pointer to the DOCSIS Ctl Thread instance.
    ///
    void CmDocsisCtlThread(BcmCmDocsisCtlThread *pThread);

    
private:

    /// Copy Constructor.  Not supported.
    BcmEpsCmDocsisApplication(const BcmEpsCmDocsisApplication &otherInstance);

    /// Assignment operator.  Not supported.
    BcmEpsCmDocsisApplication & operator = (const BcmEpsCmDocsisApplication &otherInstance);

};


//********************** Inline Method Implementations ***********************


#define SumagwPortalClient

#ifdef SumagwPortalClient

#include "SocketSet.h"
#include "Timer.h"


//********************** SumagwPortalClientThread Declaration ***********************************

class SumagwPortalClientThread : public BcmThread
{

public:

    /// Constructor.  Initializes and starts the thread.
    ///
    /// \param
    ///      initialPriority - the thread priority that should be used.
    ///
    SumagwPortalClientThread(BcmOperatingSystem::ThreadPriority initialPriority = BcmOperatingSystem::kLowNormalPriority);

	// Destructor.  Frees up any memory/objects allocated, cleans up internal
    // state.
    //
    // Parameters:  N/A
    //
    // Returns:  N/A                        
    //
    virtual ~SumagwPortalClientThread();
	
	// Returns the pointer to the singleton instance for this class.  Most
	// objects in the system will use this method rather than being passed a
	// pointer to it.  The singleton pointer will be set up in the base-class
	// constructor.
	//
	// NOTES:  This can return NULL if a singleton has not been set up for the
	//		   system, so you must check for this condition.
	//
	//		   You must not delete this object!
	//
	//		   You should not store a pointer to the object that is returned,
	//		   since it may be deleted and replaced with a new one.
	//
	// Parameters:	None.
	//
	// Returns:
	//		A pointer to the instance that should be used by the system.
	//
	static SumagwPortalClientThread *GetSingletonInstance(void);

protected:

    /// Thread constructor - this is the first method called after the thread has
    /// been spawned, and is where the thread should create all OS objects.  This
    /// has to be done here, rather than in the object's constructor, because
    /// some OS objects must be created in the context of the thread that will
    /// use them.  The object's constructor is still running in the context of
    /// the thread that created this object.
    ///
    /// The default implementation simply returns true, allowing derived classes
    /// that don't have any initialization to use the default.
    ///
    /// \retval
    ///      true if successful and ThreadMain() should be called.
    /// \retval
    ///      false if there was a problem (couldn't create an OS object, etc.)
    ///          and ThreadMain() should not be called.
    ///
    virtual bool Initialize(void);

    /// This is the main body of the thread's code.  This is the only method
    /// that absolutely must be provided in the derived class (since there is no
    /// reasonable default behavior).
    ///
    virtual void ThreadMain(void);

    /// Thread destructor - this is the last method called when the thread is
    /// exiting, and is where the thread should delete all of the OS objects that
    /// it created.
    ///
    /// The default implementation does nothing, allowing derived classes that
    /// don't have any deinitialization to use the default.
    ///
    virtual void Deinitialize(void);

public:

    //  I need to register myself with the stack so I can do DHCP, etc.
    //
    // Parameters:  None.
    //
    // Returns:  Nothing.
    //
    void RegisterWithIpStack(void);
	
    //  SumagwPortalClientEventACT call this for kEventLeaseBound event
    //  Send kAnnounceDhcpOptionBound event
    //
    // Parameters:  None.
    //
    // Returns:  Nothing.
    //
	void DhcpOptionBound(void);
	
    //  SumagwPortalClientEventACT call this for kCmDsScanStarted event
    //  need to empty my option buf to resend the option
    //
    // Parameters:  None.
    //
    // Returns:  Nothing.
    //
	void CmDsScanStart(void);
	
	// set the MessageLog level
    //
    // Parameters:  Severities. the level you want to set the MessageLog
    //
    // Returns:
    //      Nothing.
    //
	void SetMessageLogSettings(bool loglevel);
		

private:

	// Helper method to get the DHCP lease object for the stack that we are
    // controlling.
    //
    // Parameters:  None.
    //
    // Returns:
    //      A pointer to the lease object (or NULL if not found).
    //
    BcmDhcpClientLease *GetLease(void) const;

    //  handle method for kAnnounceDhcpOptionBound event
    //
    // Parameters:  None.
    //
    // Returns:  
    // -1: Dhcp option get faied
    //   0: Dhcp option not change
    //   1: Dhcp option update succeed
    //
	int ResetDhcpOption(void);

    //  handle method for kSendDhcpOption event
    //
    // Parameters:  None.
    //
    // Returns:  true of false.
    //
	bool SendDhcpOption(void);


private:

	//My self event
	enum
	{
		kExitThread,
		kErrorBackOff,
		kResetClock,
		kAnnounceDhcpOptionBound,
		kSendDhcpOption
	};

	// These are the default values that will be used for the retry parameters.
    enum
    {
        kDefaultRetryPeriod = 5*kOneMinute,     	// 5 Minutes
        kDefaultRetries = 5                         // 5 retries after 1st attempt
    };
    
	// These control the behavior of the retries.
    unsigned int fTimeBetweenRetryPeriodsMS;
    unsigned int fRetriesPerPeriod;

    // Times the period for retry sequences
    BcmTimer *pfRetryPeriodTimer;

    // Times the retries themselves
    BcmTimer *pfRetryTimer;
    
	//My SingletonInstance for others' visit
	static SumagwPortalClientThread *pfSingletonInstance;

    /// My primary IPC mechanism.
    BcmMessageQueue *pfMessageQueue;

    /// Lets me track multiple event sources.
    BcmEventSet *pfEventSet;

    // This is how the lease notifies me of events.
    BcmCompletionHandlerACT *pfDhcpAct;

	// This is where the Dhcp option store.
	BcmOctetBuffer *pfDhcpObuf;
	
	// self socketset
	BcmSocketSet *pfSocketSet;

	//WAN lease
    BcmDhcpClientLease *pfLease;

    //CmDocsisCtlThread
    BcmCmDocsisCtlThread *pfCmDocsisCtlThread;
    
};


//********************** SumagwPortalClientEventACT Declaration ***********************************

class SumagwPortalClientEventACT : public BcmCompletionHandlerACT
{
public:
   
    // Initializing Constructor. 
    //
    // Parameters:
    //      pCableHomeCtlThread - pointer to the CH Ctl thread that I must
    //                            notify.
    //
    // Returns:  N/A
    //
    SumagwPortalClientEventACT(SumagwPortalClientThread *pSumagwPortalClientThread);

    // Destructor.  Frees up any memory/objects allocated, cleans up internal
    // state.
    //
    // Parameters:  N/A
    //
    // Returns:  N/A
    //
    virtual ~SumagwPortalClientEventACT();

    // HandleEvent - in Asynchronous Completion Token pattern, this function
    //      can be used by the Initiator object (or it's BcmCallbackHandler
    //      helper object) to dispatch a specific completion handler that
    //      processes the response from an asynchronous operation.
    //
    // Parameters:
    //      eventCode - integer event code which identifies the asynchronous
    //                  event type that occurred.
    //
    // Returns:  Nothing.
    //
    virtual void HandleEvent(const BcmCompletionEvent &eventCode);

private:

    // Pointer to the client object.
    SumagwPortalClientThread *pfSumagwPortalClientThread;

};

#endif


#endif


