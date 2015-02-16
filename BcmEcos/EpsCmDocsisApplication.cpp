//****************************************************************************
//
// Copyright (c) 2003-2014 Broadcom Corporation
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
//  Filename:       EpsCmDocsisApplication.cpp
//  Author:         John McQueen
//  Creation Date:  November 21, 2003
//
//****************************************************************************

//********************** Include Files ***************************************

// My api and definitions...
#include "EpsCmDocsisApplication.h"

// BFC system configuration parameters.
#include "VersionBanner.h"

#include "BaseIpHalIf.h"
#include "BfcSystem.h"
#include "HalIf.h"
#include "IpAddress.h"

#include "CableHomeCtlThread.h"
#include "MessageLogNonVolSettings.h"
#include "HttpServerFactory.h"
#include "HttpConfigurationServerAgentFactory.h"

// Console support stuff.
#if (BFC_INCLUDE_CONSOLE_SUPPORT)
	#include "ConsoleThread.h"
	#include "CommandTable.h"
	#include "IpAddressCommandParameter.h"
#endif

// Nonvol settings for the CM app; nonvol is not optional for us, so I haven't
// bothered making it conditional.
#include "CompositeNonVolSettings.h"
#include "RgNonVolSettings.h"

// Nonvol console support; console stuff _is_ optional.
#if (BFC_INCLUDE_CONSOLE_SUPPORT) && (!BFC_CONFIG_MINIMAL_CONSOLE) && (BFC_INCLUDE_NONVOL_CONSOLE_SUPPORT)

#endif

// Old-school V2 CM Vendor Extension support.  New BFC CM Vendor Extension
// support is handled in CmDocsisSystem.cpp.
#if (BFC_CABLEHOME_VENDOR_SUPPORT)
	#include "VendorChAppFactory.h"
	#include "VendorChApplicationBase.h"
	#include "VendorChApplication.h"
#endif

#include "CableHomeDefines.h"
#include "CableHomeStatusEventCodes.h"
#include "RgGatewayIpServiceManager.h"
#include "CmDocsisCtlThread.h"
#include "DocsisModeDhcpServerDocsisStatusClientEventACT.h"
#include "CmDocsisStatusEventCodes.h"

#if (BFC_DNS_RESOLVER_SUPPORT)
	#include "DocsisModeDnsServerDocsisStatusClientEventACT.h"
#endif

#if (BFC_EROUTER_SUPPORT)
	#include "eRouterCtlThread.h"
	#include "eRouterNonVolSettings.h"
	#include "eRouterCtlThreadCableHomeStatusClientACT.h"
	#include "eRouterCtlThreadCmDocsisStatusClientACT.h"
#endif

// Version and Release info header file
#include "EpsCmDocsisSystemVersion.h"

// Component banner/version info for CableHome EPS application.
//
// NOTE TO CUSTOMERS - please don't modify this banner.  It contains version
// and copyright information for the CableHome EPS application component.  You will
// have the chance to set your own "company logo" elsewhere, and you will have
// the chance to add your own component banners/version info when the
// appropriate API is called.
//
#define kShortName "eRouter Application"

#define kBanner \
"                _/_/_/\n" \
"       _/_/    _/    _/    eRouter Dual Stack\n" \
"    _/    _/  _/    _/\n" \
"   _/_/_/_/  _/_/_/\n" \
"  _/        _/ _/\n" \
" _/        _/   _/\n" \
"  _/_/_/  _/     _/\n" \
"\n" \
"Copyright (c) 1999 - 2014 Broadcom Corporation"


// Calculate this based on build options.
#if (SNMP_SUPPORT)
	#if (0 /*BCM_FACTORY_SUPPORT - TBD */)  
		#define kSnmpFeature "SNMP w/Factory MIB Support "
	#else
		#define kSnmpFeature "SNMP "
	#endif
#else
	#define kSnmpFeature ""
#endif

#if (BFC_CABLEHOME_VENDOR_SUPPORT)
	#define kVendorFeature "Customer Extension "
#else
	#define kVendorFeature ""
#endif

#if (BFC_EROUTER_SUPPORT)
	#define keRouterFeature "eRouter "
#else
	#define keRouterFeature ""
#endif

#if (BFC_VLAN_INCLUDED)
	#define kVLANFeature "VLAN "
#else
	#define kVLANFeature ""
#endif

#if (NAT_HW_ACCELERATION)
	#define kNatpFeature "NATP "
#else
	#define kNatpFeature ""
#endif

#if (BCM_DSLITE_SUPPORT)
	#define kDsliteFeature "DS-Lite "
#else
	#define kDsliteFeature ""
#endif

#if (BCM_L2OGRE_SUPPORT)
	#define kL2oGreFeature "L2oGRE "
#else
	#define kL2oGreFeature ""
#endif

#if (BCM_HOME_HOTSPOT_SUPPORT)
	#define kHomeHotspotFeature "HomeHotspot "
#else
	#define kHomeHotspotFeature ""
#endif


// UNFINISHED need to build this based on the compile time options.
#define kFeatures keRouterFeature kSnmpFeature kVendorFeature kNatpFeature kVLANFeature kDsliteFeature kL2oGreFeature kHomeHotspotFeature


//********************** Local Types *****************************************

//********************** Local Constants *************************************

enum
{
	kPrimaryParameter = 1,
	kSecondaryParameter
};

//********************** Local Variables *************************************


#if 0 // (BFC_INCLUDE_CONSOLE_SUPPORT)

static unsigned int gCommandIdBase = 0;

#endif

//********************** Local Functions *************************************

#if 0 // (BFC_INCLUDE_CONSOLE_SUPPORT)

static void CommandHandler(void *pInstanceValue, const BcmCommand &command);

#endif



//********************** Class Method Implementations ************************


/// Default Constructor.  Initializes the state of the object...
///
/// Note that the constructor is called fairly early on in system startup.
/// This places some limits on what can be done here, especially if there
/// is any dependency on other system components.  We recommend that the
/// work done here be limited to setting variables to default values, setting
/// pointers to NULL, etc, and leaving the real work to the initialization
/// API.
///
BcmEpsCmDocsisApplication::BcmEpsCmDocsisApplication( bool dualForwarder ) :
BcmCableHomeCommonApplication("EPS-CM DOCSIS Application", dualForwarder ),
pfCmDocsisCtlThread(NULL),
pfCableHomeCmDocsisStatusClientACT(NULL)
{
	CallTrace("BcmEpsCmDocsisApplication", "Constructor");

	fMessageLogSettings.SetModuleName("BcmEpsCmDocsisApplication");

	// Create the old-school V2 translation layer object, giving it a pointer
	// to me so that it will call me.

	// This should be deleted in the base class
	pfCableHomeApplication = new BcmCableHomeApplication(this);

}



/// Destructor.  Frees up any memory/objects allocated, cleans up internal
/// state.
///
BcmEpsCmDocsisApplication::~BcmEpsCmDocsisApplication()
{
	CallTrace("BcmEpsCmDocsisApplication", "Destructor");
}


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
void BcmEpsCmDocsisApplication::AddVersionInfo(BcmVersionBanner *pVersionBanner)
{
	CallTrace("BcmEpsCmDocsisApplication", "AddVersionInfo");

	pVersionBanner->AddComponentInfo(kBanner, kVersion, 
									 pVersionBanner->IsRelease(kReleaseState), 
									 kFeatures,
									 kShortName);

	// Let the vendor extension add it's own version info.
#if (BFC_CABLEHOME_VENDOR_SUPPORT)
	if ( pfVendorChApplication != NULL )
	{
		pfVendorChApplication->AddVersionInfo(pVersionBanner);
	}
#endif

	// If we have factory support, create a factory base group bridge.
#if ((BCM_FACTORY_SUPPORT) && (SNMP_SUPPORT))
	{
		// TBD
	}
#endif
}



/// Called by the system object when it is time to shut everything down in
/// preparation for exiting/rebooting the system.  This is a good place to
/// stop threads, deregister things, etc.  You could wait to do this in your
/// destructor, but by then other object may already be destroyed.  This
/// method is called before anything is destroyed, so you know your pointers
/// should still be valid.
///
void BcmEpsCmDocsisApplication::DeInitialize(void)
{
	CallTrace("BcmEpsCmDocsisApplication", "DeInitialize");

	if ( pfCableHomeCmDocsisStatusClientACT )
	{
		delete pfCableHomeCmDocsisStatusClientACT;
		pfCableHomeCmDocsisStatusClientACT = NULL;
	}

	BcmCableHomeCommonApplication::DeInitialize();

}


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
void BcmEpsCmDocsisApplication::RebootApplication(void)
{
	CallTrace("BcmEpsCmDocsisApplication", "RebootEps");

	if ( pfCmDocsisCtlThread )
	{
		pfCmDocsisCtlThread->Shutdown("Resetting due to EPS reboot");
	}
} 



///
///  CreateAndConfigureCableHome:
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
void BcmEpsCmDocsisApplication::CreateAndConfigureCableHome(void) 
{
	CallTrace("BcmEpsCmDocsisApplication", "CreateAndConfigureCableHome");

	// Let the base class initialize first and follow up with 
	// EPS specific CableHome items.
	//
	{
		BcmCableHomeCommonApplication::CreateAndConfigureCableHome();
	}


#if (BFC_EROUTER_SUPPORT)
	if ( (BcmeRouterIpProvisioningMode)BcmeRouterNonVolSettings::GetSingletonInstance()->IpProvisioningMode() != keRouterDisabled )
#else
	BcmRgNonVolSettings *pRgNonVolSettings = BcmRgNonVolSettings::GetSingletonInstance();
	if ( pRgNonVolSettings->RgModeEnabled() == true )
#endif
	{
		// Register with the DOCSIS Control thread to receive DOCSIS events 
		// for the CableHomeCtlThread.
		//
		RegisterDocsisCtlThreadEvents(pfCmDocsisCtlThread);
	}
	else
	{
		RegisterDocsisCtlThreadEvents(pfCmDocsisCtlThread);

		gAlwaysMsg(fMessageLogSettings, "CreateAndConfigureCableHome")
		<< "Running as a DOCSIS data-only device!" << endl;

		BcmRgGatewayIpServiceManager *pRgGatewayIpServiceManager = BcmRgGatewayIpServiceManager::GetSingletonInstance();

		// Start DOCSIS DHCP Server Service on the 192.168.100.1 interface
		//
		if ( pRgGatewayIpServiceManager->InitDocsisModeDhcpServerService(
																		(BcmBaseIpHalIf *)pfIpStackHalIf[kCMPrivateStackInterface-1]) == false )
		{
			gErrorMsg(fMessageLogSettings, "CreateAndConfigureCableHome")
			<< "Unable to initialize Docsis Dhcp server service!" << endl;
			assert(0);
		}

		if ( pRgGatewayIpServiceManager->StartDocsisModeDhcpServerService() == false )
		{
			gErrorMsg(fMessageLogSettings, "CreateAndConfigureCableHome")
			<< "Unable to start Docsis Dhcp server service!" << endl;
			assert(0);
		}

		BcmDocsisModeDhcpServerDocsisStatusClientEventACT *pDocsisDhcpServerClient = 
		new BcmDocsisModeDhcpServerDocsisStatusClientEventACT();

		if ( pfCmDocsisCtlThread )
		{
			pfCmDocsisCtlThread->SubscribeEventNote(
												   (unsigned int)BcmCmDocsisStatusEventCodes::kCmIsOperational,pDocsisDhcpServerClient);

			pfCmDocsisCtlThread->SubscribeEventNote(
												   (unsigned int)BcmCmDocsisStatusEventCodes::kCmIsNotOperational,pDocsisDhcpServerClient);
		}

#if (BFC_INCLUDE_HTTP_SERVER_SUPPORT)
		// start HTTP Server.
		BcmBaseIpHalIf * pCmPrivateStack = (BcmBaseIpHalIf *)BcmIpStackManager::GetSingleton().Find(BcmMessageLogNonVolSettings::GetSingletonInstance()->RemoteAccessIpStackNumber());

		BcmHttpServerThread * pHttpServerThread = BcmHttpServerFactory::NewHttpServer(pCmPrivateStack,(void*)pfCmDocsisCtlThread);

		BcmHttpRequestAgent * pHttpRequestAgent = BcmHttpConfigurationServerAgentFactory::NewHttpConfigurationServerAgent(kHttpAgentCm,(void*)pfCmDocsisCtlThread);
		pHttpServerThread->RegisterHttpAgent(pHttpRequestAgent,kHttpGet|kHttpPost);
#endif

#if (BFC_DNS_RESOLVER_SUPPORT)
		// start DOCSIS-mode DNS resovler
		// Create and configure the CableHome DNS server Service
		if ( pRgGatewayIpServiceManager->InitDocsisModeDnsService((BcmBaseIpHalIf *)pfIpStackHalIf[kCMStackInterface-1]) == false )
		{
			gErrorMsg(fMessageLogSettings, "PostSnmpCableHomeConfiguration") 
			<< "Failed to initialize CableHome DNS server service!" << endl;
		}

		//
		// Start the DNS Service
		//
		if ( pRgGatewayIpServiceManager->StartDocsisModeDnsService() == false )
		{
			gErrorMsg(fMessageLogSettings, "PostSnmpCableHomeConfiguration")
			<< "Unable to start Dns service!" << endl;
		}

		BcmDocsisModeDnsServerDocsisStatusClientEventACT *pDocsisDnsServerClient = 
		new BcmDocsisModeDnsServerDocsisStatusClientEventACT();

		if ( pfCmDocsisCtlThread )
		{
			pfCmDocsisCtlThread->SubscribeEventNote(
												   (unsigned int)BcmCmDocsisStatusEventCodes::kCmIsOperational,pDocsisDnsServerClient);
		}
#endif
#if (BCM3382MVG) || (BCM3382WxG)
		StartTftpServer();
#endif

	}
	
#ifdef SumagwPortalClient
	SumagwPortalClientThread *pSumagwPortalClientThread = new SumagwPortalClientThread();

	if ( pSumagwPortalClientThread == NULL  )
	{
		gErrorMsg(fMessageLogSettings, "CreateAndConfigureCableHome") 
		<< "pSumagwPortalClientThread pointer is null!" << endl; 
	}
	else
	{
		// Register with the IpStack lease to receive DHCP events 
		// for the SumagwPortalClientThread.
		//
		pSumagwPortalClientThread->RegisterWithIpStack();
	}
#endif
}



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
void BcmEpsCmDocsisApplication::PostSnmpCableHomeConfiguration(void)
{
	CallTrace("BcmEpsCmDocsisApplication", "PostSnmpCableHomeConfiguration");

#if (BFC_EROUTER_SUPPORT)
	if ( (BcmeRouterIpProvisioningMode)BcmeRouterNonVolSettings::GetSingletonInstance()->IpProvisioningMode() != keRouterDisabled )
#else
	if ( BcmRgNonVolSettings::GetSingletonInstance()->RgModeEnabled() == true )
#endif
	{

		// Let the base class initialize first and follow up with 
		// EPS specific CableHome items.
		//
		{
			BcmCableHomeCommonApplication::PostSnmpCableHomeConfiguration();
		}

		BcmRgGatewayIpServiceManager *pRgGatewayIpServiceManager = BcmRgGatewayIpServiceManager::GetSingletonInstance();

		// Start the CableHome DHCP server thread
		if ( pRgGatewayIpServiceManager->StartCableHomeDhcpServerService() == false )
		{
			gErrorMsg(fMessageLogSettings, "PostSnmpCableHomeConfiguration") 
			<< "Unable to start CableHome Dhcp server service!" << endl;
		}
	}
	// Let's now add the DOCSIS CM side mibs to SNMP agent...
	RegisterCmSideMibs();
}




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
void BcmEpsCmDocsisApplication::RegisterDocsisCtlThreadEvents(BcmCmDocsisCtlThread *pDocsisCtrlThread)
{
	CallTrace("BcmEpsCmDocsisApplication", "RegisterDocsisCtlThreadEvents");


	if ( pDocsisCtrlThread )
	{
		BcmCableHomeCtlThread *pCableHomeCtlThread = BcmCableHomeCtlThread::GetSingletonInstance();

		BcmCableHomeCtrlThreadCmDocsisStatusClientACT *pfCableHomeCmDocsisStatusClientACT 
		= new BcmCableHomeCtrlThreadCmDocsisStatusClientACT( pCableHomeCtlThread );

		pDocsisCtrlThread->SubscribeEventNote((unsigned int)BcmCmDocsisStatusEventCodes::kCmPreRegConfigFileOk,
											  pfCableHomeCmDocsisStatusClientACT);


#if (BFC_EROUTER_SUPPORT)
		if ( (BcmeRouterIpProvisioningMode)BcmeRouterNonVolSettings::GetSingletonInstance()->IpProvisioningMode() != keRouterDisabled )
		{
#endif
			pDocsisCtrlThread->SubscribeEventNote((unsigned int)BcmCmDocsisStatusEventCodes::kCmIsOperational,
												  pfCableHomeCmDocsisStatusClientACT);

            // Use kResetIp rather than kCmIsNotOperational here.  ResetIp is a 
            // better choice because it will be published by the CM a little
            // earlier in the reset process, which allows the DHCP release message
            // to be sent out before other things get shut down which would block
            // the message.
			pDocsisCtrlThread->SubscribeEventNote((unsigned int)BcmCmDocsisStatusEventCodes::kCmResetIp/*kCmIsNotOperational*/,
												  pfCableHomeCmDocsisStatusClientACT);

			pDocsisCtrlThread->SubscribeEventNote((unsigned int)BcmCmDocsisStatusEventCodes::kCmTodInitOk,
												  pfCableHomeCmDocsisStatusClientACT);


#if (BFC_EROUTER_SUPPORT)

			BcmeRouterCtlThread *peRouterCtlThread = BcmeRouterCtlThread::GetSingletonInstance();

			BcmeRouterCtlThreadCableHomeStatusClientACT *pfeRouterCableHomeStatusClientACT 
			= new BcmeRouterCtlThreadCableHomeStatusClientACT( peRouterCtlThread );

			pCableHomeCtlThread->SubscribeEventNote((unsigned int)BcmCableHomeStatusEventCodes::kChOperational,
													pfeRouterCableHomeStatusClientACT);

			pCableHomeCtlThread->SubscribeEventNote((unsigned int)BcmCableHomeStatusEventCodes::kChNotOperational,
													pfeRouterCableHomeStatusClientACT);


			BcmeRouterCtlThreadCmDocsisStatusClientACT *pfeRouterCmStatusClientACT 
			= new BcmeRouterCtlThreadCmDocsisStatusClientACT( peRouterCtlThread );

			pDocsisCtrlThread->SubscribeEventNote((unsigned int)BcmCmDocsisStatusEventCodes::kCmResetIp,
												  pfeRouterCmStatusClientACT);

		}
#endif

	}
}


/// Gets the DOCSIS Ctl Thread instance that is being used.
///
/// \return
///      A pointer to the DOCSIS Ctl Thread instance.
///
BcmCmDocsisCtlThread * BcmEpsCmDocsisApplication::CmDocsisCtlThread(void) const 
{
	CallTrace("BcmEpsCmDocsisApplication", "CmDocsisCtlThread");

	return pfCmDocsisCtlThread;
}

/// Sets the DOCSIS Ctl Thread instance that is being used.
///
/// \return
///      A pointer to the DOCSIS Ctl Thread instance.
///
void BcmEpsCmDocsisApplication::CmDocsisCtlThread(BcmCmDocsisCtlThread *pThread)
{
	CallTrace("BcmEpsCmDocsisApplication", "CmDocsisCtlThread");

	pfCmDocsisCtlThread = pThread;
}


#if 0 // (BFC_INCLUDE_CONSOLE_SUPPORT)

static void CommandHandler(void *pInstanceValue, const BcmCommand &command)
{
	const BcmCommandParameter *pParameter = NULL;

	if ( command.pfCommandParameterList != NULL )
	{
		pParameter = command.pfCommandParameterList->Find(kPrimaryParameter);
	}

	switch ( command.fCommandId - gCommandIdBase )
	{
	#if (!BFC_CONFIG_MINIMAL_CONSOLE)

	#endif

	default:
		gLogMessageRaw 
		<< "WARNING - unknown command id (" << command.fCommandId << ")!  Ignoring...\n";
		break;
	}
}

#endif

#ifdef SumagwPortalClient

//********************** Include Files ***************************************

#include "SocketFactory.h"
#include "EventSet.h"
#include "arpa/inet.h"


//********************** Local Types *****************************************

#define SUMAGW_MSG_LEN	(512)
#define SUMAGW_MSG_DHCP (0x00102001)

#define SRV_PORT 7759

typedef struct {
	int  msg;
	int  ret;
	int  len;
	char arg[SUMAGW_MSG_LEN];
}sumagw_msg_t;


//********************** Local Variables *************************************
// Set this to NULL initially.  This will be set up in my constructor.

SumagwPortalClientThread  *SumagwPortalClientThread::pfSingletonInstance = NULL;


//********************** SumagwPortalClientThread Method Implementations ************************

SumagwPortalClientThread::SumagwPortalClientThread(BcmOperatingSystem::ThreadPriority initialPriority):
    BcmThread("SumagwPortalClientThread", false, initialPriority),
		
	//create all OS objects in Initialize(), such as EventSet, MessageQueue etc... 
    pfMessageQueue(NULL),
    pfEventSet(NULL),
    pfDhcpAct(NULL)
{
	CallTrace("SumagwPortalClientThread", "SumagwPortalClientThread");
	fMessageLogSettings.SetModuleName("SumagwPortalClientThread");

	// set MessageLog Severities
	uint32 Severities = BcmMessageLogSettings::kNoMessages;
	fMessageLogSettings.SetEnabledSeverities(Severities);

	// set MessageLog Fields
	uint8 Fields = BcmMessageLogSettings::kThreadIdField
				| BcmMessageLogSettings::kModuleNameField
				| BcmMessageLogSettings::kFunctionNameField
				| BcmMessageLogSettings::kInstanceNameField
				| BcmMessageLogSettings::kSeverityField;
	fMessageLogSettings.SetEnabledFields(Fields);

	// Set up the singleton.
    if ( pfSingletonInstance != NULL )
    {
        gWarningMsg(fMessageLogSettings, "SumagwPortalClientThread")
        << "Singleton pointer is not NULL!  There are multiple BcmeRouterCtlThread devices! "
        	<< "Leaving the singleton pointer alone..." << endl;
    }
    else
    {
        gInfoMsg(fMessageLogSettings, "SumagwPortalClientThread")
        << "Setting up singleton pointer." << endl;

        pfSingletonInstance = this;
    }

	//initialize my timer constant
	fTimeBetweenRetryPeriodsMS = kDefaultRetryPeriod;
    fRetriesPerPeriod = kDefaultRetries;
	pfRetryPeriodTimer = NULL;
    pfRetryTimer = NULL;
	
    if (!pfOperatingSystem->BeginThread(this, 4 * 1024))
    {
        gFatalErrorMsg(fMessageLogSettings, "SumagwPortalClientThread")
            << "Failed to spawn thread!" << endl;

        assert(0);
    }
	
	//create self objects here
    pfDhcpObuf = new BcmOctetBuffer(SUMAGW_MSG_LEN);
	pfSocketSet = new BcmSocketSet("Sumagw Portal Client SocketSet");

	//public object, do not delete
	pfLease = GetLease();
	pfCmDocsisCtlThread = BcmCmDocsisCtlThread::GetSingletonInstance();
	
}

SumagwPortalClientThread::~SumagwPortalClientThread()
{
    CallTrace("SumagwPortalClientThread", "~SumagwPortalClientThread");

	if ( pfSingletonInstance == this )
    {
        gInfoMsg(fMessageLogSettings, "~SumagwPortalClientThread")
        << "Clearing the singleton pointer." << endl;

        pfSingletonInstance = NULL;
    }
    else
    {
        gWarningMsg(fMessageLogSettings, "~SumagwPortalClientThread")
        << "I'm not the singleton instance!  Leaving the singleton pointer alone..." << endl;
    }

	// Tell the thread to exit.
    gInfoMsg(fMessageLogSettings, "~SumagwPortalClientThread") 
    			<< "Signalling the thread to exit..." << endl;
	
    // Tell myself to exit.
    if (pfMessageQueue != NULL)
    {
        pfMessageQueue->Send(kExitThread);
    }

	//Delete all OS objects in Deinitialize()
	//Delete self objects here
	
    if ( pfDhcpAct != NULL )
    {
        delete pfDhcpAct;
        pfDhcpAct = NULL;
    }
	
    if ( pfDhcpObuf != NULL )
    {
        delete pfDhcpObuf;
        pfDhcpObuf = NULL;
    }
	
    if ( pfSocketSet != NULL )
    {
        delete pfSocketSet;
        pfSocketSet = NULL;
    }

    WaitForThread();
}

SumagwPortalClientThread *SumagwPortalClientThread::GetSingletonInstance(void)
{
    if ( pfSingletonInstance == NULL )
    {
        gLogMessageRaw
        << "SumagwPortalClientThread::GetSingletonInstance:  WARNING - the singleton is NULL" <<
        ", and someone is accessing it!" << endl;
    }

    return(pfSingletonInstance);
}

void SumagwPortalClientThread::SetMessageLogSettings(bool loglevel)
{    
	// set MessageLog Severities
	if(loglevel)
	{
		uint32 Severities = BcmMessageLogSettings::kFatalErrorMessages 
					| BcmMessageLogSettings::kErrorMessages 
					| BcmMessageLogSettings::kWarningMessages
					| BcmMessageLogSettings::kInformationalMessages;
		
		fMessageLogSettings.SetEnabledSeverities(Severities);
	}
	else
	{
		uint32 Severities = BcmMessageLogSettings::kNoMessages;
		fMessageLogSettings.SetEnabledSeverities(Severities);
	}
}


bool SumagwPortalClientThread::Initialize(void)
{
	CallTrace("SumagwPortalClientThread", "Initialize");
	
    pfMessageQueue = pfOperatingSystem->NewMessageQueue("Sumagw Portal Client Thread's message queue");

    pfEventSet = pfOperatingSystem->NewEventSet("Sumagw Portal Client Thread's event set");

    pfEventSet->Add(*pfMessageQueue);

	pfRetryPeriodTimer = pfOperatingSystem->NewTimer("Sumagw Portal Retry Period Timer");
	
	pfRetryTimer = pfOperatingSystem->NewTimer("Sumagw Portal Retry Timer");


    return true;
}

void SumagwPortalClientThread::Deinitialize(void)
{
	CallTrace("SumagwPortalClientThread", "Deinitialize");

    delete pfRetryPeriodTimer;
    pfRetryPeriodTimer = NULL;

    delete pfRetryTimer;
    pfRetryTimer = NULL;

    pfEventSet->RemoveAll();
    
    delete pfEventSet;
    pfEventSet = NULL;

    delete pfMessageQueue;
    pfMessageQueue = NULL;
}

void SumagwPortalClientThread::ThreadMain(void)
{
	CallTrace("SumagwPortalClientThread", "ThreadMain");

    gInfoMsg(fMessageLogSettings, "ThreadMain")
        << "Sumagw Portal Client starting..." << endl;

	// Clear pending events in the timer, and start the retry period timer.
	pfRetryPeriodTimer->Stop();
	pfRetryPeriodTimer->GetEvent()->Clear();
	
	pfRetryTimer->Stop();
	pfRetryTimer->GetEvent()->Clear();
	
	// if the client specified a retry period, start the timer now
	if( fTimeBetweenRetryPeriodsMS != 0 ) 
	{
		pfRetryPeriodTimer->Start(fTimeBetweenRetryPeriodsMS, BcmTimer::kRepeat);
	}

	//Initialize the clock
	unsigned int timeBetweenRetriesMS = 2*kOneSecond;
	unsigned int numberOfRetries = 0;

	bool timeToExit = false;
    bool result = false;

    while (!timeToExit)
	{
        // Wait for an event to occur.
        result = pfEventSet->Wait();

        // Since I'm waiting forever for any event to occur, if this returns
        // with false, then there was a big problem that's likely to persist
        // forever.
        if ( result == false )
        {
            gErrorMsg(fMessageLogSettings, "ThreadMain") << "Failed to wait for the event set!" << endl;
            break;
        }

        // See if we were awakened in order to exit.
        if ( timeToExit )
        {
            gAlwaysMsg(fMessageLogSettings, "ThreadMain") << "Exiting Thread." << endl;
            break;
        }
	
		// Process messages in my queue first.
		if (pfEventSet->Occurred(*pfMessageQueue))
		{
			unsigned int messageCode;
			void *pMessageData = NULL;
			int result = -1;
				
			while (pfMessageQueue->Receive(messageCode, pMessageData))
			{
				switch (messageCode)
				{
                    case kExitThread:
                        // Set this flag to true, but don't bail out on anything
                        // else.  Interestingly this causes the message queue
                        // to be drained naturally and all resources to be
                        // released, so we don't need to do anything special to
                        // clean up.
						gInfoMsg(fMessageLogSettings, "ThreadMain")
							<< "ThreadMain exiting!" << endl;
                        timeToExit = true;
                        break;

                    case kErrorBackOff:
						// exponential backoff with exponent of two, pegged out at 256 seconds
					    gWarningMsgNoFields(fMessageLogSettings)
					    << "Starting Sumagw backoff timer for " << timeBetweenRetriesMS/1000 
					    	<< " seconds ..." << endl;
						
						//wait for timeBetweenRetriesMS
					    pfRetryTimer->Start(timeBetweenRetriesMS);
					    pfRetryTimer->Wait();

						//Exponential Backoff algorithm
					    timeBetweenRetriesMS *= 2;
					    if( timeBetweenRetriesMS > 256*kOneSecond )
					    {
					        timeBetweenRetriesMS = 256*kOneSecond;
					    }

						// if retries per period was specified, check to see if we have hit 
				        // the max before waiting on the period timer to expire
				        if( (fRetriesPerPeriod != 0) && (numberOfRetries >= fRetriesPerPeriod) ) 
				        {
				            gWarningMsgNoFields(fMessageLogSettings)
				            << "Waiting for Sumagw Portal retry period timer to expire..." << endl;

				            // Wait for the timer to expire
				            if( pfRetryPeriodTimer->Wait() == true )
				            {
				                gWarningMsg(fMessageLogSettings, "ThreadMain")
				                << "Period timer expired.  Trying Sumagw Portal again...\n" << endl;
				    
				                // reset time between retries timer back to 1 second not that we are
				                // starting a new interval
				                timeBetweenRetriesMS = 2*kOneSecond;
				                numberOfRetries = 0;
				            }
				        }
						
						//Retry numbers +1
						numberOfRetries++;
                        break;
					
                    case kResetClock:
						timeBetweenRetriesMS = 2*kOneSecond;
						numberOfRetries = 0;
                        break;
						
                    case kAnnounceDhcpOptionBound:
						result = ResetDhcpOption();
						if(-1 == result)
						{
							//some problem in ResetDhcpOption(), need REDO
							gInfoMsg(fMessageLogSettings, "ThreadMain")
								<< "Dhcp option get faied!" << endl;
							pfMessageQueue->Send(kErrorBackOff);
							pfMessageQueue->Send(kAnnounceDhcpOptionBound);
						}
						else if(1 == result)
						{
							//Dhcp option changed and update succeed, need send new option
							gInfoMsg(fMessageLogSettings, "ThreadMain")
								<< "Dhcp option update succeed!" << endl;
							pfOperatingSystem->Sleep(250 * kOneMillisecond);
							pfMessageQueue->Send(kSendDhcpOption);
						}
						else	//0 == result
						{
							//Dhcp option not change, Do nothing
							gInfoMsg(fMessageLogSettings, "ThreadMain")
								<< "Dhcp option not change!" << endl;
						}
                    	break;
					
                    case kSendDhcpOption:
						result = SendDhcpOption();
						if(true == result)
						{
							//send succeed, Do nothing here
							gInfoMsg(fMessageLogSettings, "ThreadMain")
								<< "New Dhcp option has been sent!" << endl;

							//reset the clock
							pfOperatingSystem->Sleep(250 * kOneMillisecond);
							pfMessageQueue->Send(kResetClock);
						}
						else
						{
							//some problem in SendDhcpOption(), need REDO
							gInfoMsg(fMessageLogSettings, "ThreadMain")
								<< "New Dhcp option send failed!" << endl;
							
							pfMessageQueue->Send(kErrorBackOff);
							pfMessageQueue->Send(kSendDhcpOption);
						}
						break;
						
                    default:
						break;
				}
			}
		}

		// Stop the timers and clear any pending events.
    	pfRetryPeriodTimer->Stop();
    	pfRetryTimer->Stop();
	}
}

bool SumagwPortalClientThread::SendDhcpOption()
{
	CallTrace("SumagwPortalClientThread", "SendDhcpOption");
	
	gWarningMsg(fMessageLogSettings, "SendDhcpOption")
		<< "TCP client is online!" << endl;

	// new socket, must use new, otherwize the old socket CAN NOT connect again
    BcmSocket* pSocket = BcmSocketFactory::NewSocket(AF_INET, SOCK_STREAM, TCP);
	if (NULL == pSocket)
	{
		gWarningMsg(fMessageLogSettings, "SendDhcpOption")
			<< "Failed to Create server! errno: " << errno << endl;
		return false;
	}

	// connect 7429 server
	BcmIpAddress serverIpAddress("192.168.88.2");
	if (0 != pSocket->Connect(serverIpAddress, SRV_PORT))
	{
		gWarningMsg(fMessageLogSettings, "SendDhcpOption")
			<< "Failed to Connect server! errno: " << errno << endl
			<< "the serverIpAddress: " << serverIpAddress 
			<< ", the port: " << SRV_PORT << endl;
		
        pSocket->Close();
		if( pSocket != NULL )
		{
			delete pSocket;
			pSocket = NULL;
		}
		
		return false;
	}

	// combine the send message
	sumagw_msg_t sumagw_msg;
	
	memset(&sumagw_msg, 0x00, sizeof(sumagw_msg));
	sumagw_msg.msg = SUMAGW_MSG_DHCP;
	sumagw_msg.len = SUMAGW_MSG_LEN;
	sumagw_msg.ret = 0;
	memcpy(sumagw_msg.arg, pfDhcpObuf->AccessBuffer(), pfDhcpObuf->BytesUsed());
	sumagw_msg.len = pfDhcpObuf->BytesUsed();
		
	sumagw_msg.msg = htonl(sumagw_msg.msg);
	sumagw_msg.ret = htonl(sumagw_msg.ret);
	sumagw_msg.len = htonl(sumagw_msg.len);
	
	gWarningMsg(fMessageLogSettings, "SendDhcpOption")
		<< "ready to send data..." 
		<< "sumagw_msg.msg: " << sumagw_msg.msg 
		<< ", sumagw_msg.ret: " << sumagw_msg.ret
		<< ", sumagw_msg.len: " << sumagw_msg.len
		<< ", sumagw_msg.arg: " << sumagw_msg.arg << endl;

	pfSocketSet->ClearAll();
	
	// Set to write the socket create above
	pfSocketSet->SetWrite(pSocket);

	// timeout set to 3 second
	pfSocketSet->Select(3000);
	if (pfSocketSet->IsSetWrite(pSocket))
	{	
		// send new dhcp option to 7429
		int sendbytes = (int)(sizeof(sumagw_msg_t)+sumagw_msg.len-SUMAGW_MSG_LEN);
		int nbytes = pSocket->Send((char *)&sumagw_msg, sendbytes, 0);
		
		gWarningMsg(fMessageLogSettings, "SendDhcpOption")
			<< "send data bytes is: " << nbytes 
			<< ", recommend bytes is " << sendbytes << endl;
		
	    if(nbytes != sendbytes)
		{
			gWarningMsg(fMessageLogSettings, "SendDhcpOption")
		     << "Not all bytes written! errno: " << errno << endl;
			
			pSocket->Close();
			if( pSocket != NULL )
			{
				delete pSocket;
				pSocket = NULL;
			}
			
		    return false;
		}

	    // Sleep for a bit to allow the server to respond.
	    //
	    // PR2824 - give more time for the reply to come back.  In many
	    // cases, this is the first packet to hit the network, which
	    // means the stack needs to ARP, which can take a while to
	    // complete.
	    pfOperatingSystem->Sleep(250 * kOneMillisecond);

		//Applivation-level acknowledge, To confirm the send message has received complete
		//recv MUST be BLOCK
		
	    char buffer[1] = {0};
		nbytes = pSocket->Recv((char *)buffer, 1, 0);
		gWarningMsg(fMessageLogSettings, "SendDhcpOption")
 			<< "recv data bytes is: " << nbytes << endl;
		
	    if(nbytes != 1)
	    {
			gWarningMsg(fMessageLogSettings, "SendDhcpOption")
	         << "Applivation-level ack failed! errno: " << errno << endl;
			
			pSocket->Close();
			if( pSocket != NULL )
			{
				delete pSocket;
				pSocket = NULL;
			}
			
	        return false;
	    }
	}
	
	pSocket->Close();
	if( pSocket != NULL )
	{
		delete pSocket;
		pSocket = NULL;
	}

    return true;
}

//cscfg:URL=http://10.10.20.123/stb/configs/netwok.cfg&cfgmd5=E781FD
int SumagwPortalClientThread::ResetDhcpOption(void)
{
	CallTrace("SumagwPortalClientThread", "ResetDhcpOption");

	//Dhcp option get faied, need REDO
    int ret_val = -1;

    BcmOctetBuffer pDhcpObuf(SUMAGW_MSG_LEN);
	pDhcpObuf.Empty();

    if (pfLease != NULL)
    {
        for(int i=151; i<=155; i++ ) 
		{
            if (pfLease->ServerLeaseSettings().GetOption((DhcpRfc2132OptionCodes)i, pDhcpObuf) ) 
			{   
				if(*pfDhcpObuf != pDhcpObuf)
				{
					pfDhcpObuf->Empty();
					if(pfDhcpObuf->Append(pDhcpObuf))
					{
						//Dhcp option changed and update succeed, need send new option
						ret_val = 1;
					}
				}
				else
				{
					//Dhcp option not change, Do nothing
					ret_val = 0;
				}
                break;
            }
        }
    }

    return ret_val;
}

BcmDhcpClientLease *SumagwPortalClientThread::GetLease(void) const
{
    CallTrace("SumagwPortalClientThread", "GetLease");

    // Find my IP stack and then find the lease.
    BcmBaseIpHalIf *pIpHalIf;

    pIpHalIf = BcmIpStackManager::GetSingleton().Find(kWanManStackInterface);

    if (pIpHalIf == NULL)
    {
        gErrorMsg(fMessageLogSettings, "GetLease")
            << "Can't find IP stack " << kWanManStackInterface
            << "!  Can't find lease!" << endl;

        return NULL;
    }

    BcmDhcpClientId clientId(pIpHalIf->MacAddress());

    return pIpHalIf->DhcpClientIf()->FindLease(clientId);
}

void SumagwPortalClientThread::RegisterWithIpStack(void)
{
	CallTrace("SumagwPortalClientThread", "RegisterWithIpStack");

    if (pfDhcpAct != NULL)
    {
        gWarningMsg(fMessageLogSettings, "RegisterWithIpStack")
            << "Already registered with the IP stack; ignoring..." << endl;

        return;
    }

    if (pfLease != NULL)
    {
        // Create my ACT.
        pfDhcpAct = new SumagwPortalClientEventACT(this);

        // Register my act with this lease so that I get called with
        // events.
        pfLease->Subscribe(BcmDhcpClientLease::kEventLeaseBound, pfDhcpAct);
		
		pfCmDocsisCtlThread->SubscribeEventNote(BcmCmDocsisStatusEventCodes::kCmDsScanStarted, // Scan starting
												pfDhcpAct);
    }
    else
    {
        gErrorMsg(fMessageLogSettings, "RegisterWithIpStack")
            << "Can't find the lease associated with my IP stack!  Can't configure the DHCP client!" << endl;

        return;
    }
}

void SumagwPortalClientThread::DhcpOptionBound()
{
	CallTrace("SumagwPortalClientThread", "DhcpOptionBound");

    pfMessageQueue->Send(kAnnounceDhcpOptionBound);
}

void SumagwPortalClientThread::CmDsScanStart()
{
	CallTrace("SumagwPortalClientThread", "CmDsScanStart");

	gWarningMsg(fMessageLogSettings, "CmDsScanStart")
		<< "CM try to lock ds, need to empty my option buf" << endl;
	
	pfDhcpObuf->Empty();
}


//********************** SumagwPortalClientEventACT Method Implementations ************************

SumagwPortalClientEventACT::SumagwPortalClientEventACT(SumagwPortalClientThread *pSumagwPortalClientThread) :
    BcmCompletionHandlerACT(),
    pfSumagwPortalClientThread(pSumagwPortalClientThread)
{
	CallTrace("SumagwPortalClientEventACT", "SumagwPortalClientEventACT");
	
    // Nothing else to do.
}

SumagwPortalClientEventACT::~SumagwPortalClientEventACT()
{
	CallTrace("SumagwPortalClientEventACT", "~SumagwPortalClientEventACT");
	
    // Clear the pointer, but don't delete the object.
    pfSumagwPortalClientThread = NULL;
}

void SumagwPortalClientEventACT::HandleEvent(const BcmCompletionEvent &EventCode)
{
	CallTrace("SumagwPortalClientEventACT", "~HandleEvent");
	
	// evaluate event_code and make a call back to our client object.
	switch( EventCode )
	{
		case BcmDhcpClientLease::kEventLeaseBound:
			pfSumagwPortalClientThread->DhcpOptionBound();
			break;
		
		case BcmCmDocsisStatusEventCodes::kCmDsScanStarted:
			pfSumagwPortalClientThread->CmDsScanStart();
			break;
		default:
			break;
	}
}




#endif




