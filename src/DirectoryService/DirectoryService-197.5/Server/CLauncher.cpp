/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CLauncher
 */

#include "CLauncher.h"
#include "ServerControl.h"
#include "DSCThread.h"
#include "PrivateTypes.h"
#include "CLog.h"
#include "CServerPlugin.h"
#include "PluginData.h"
#include "CPlugInList.h"
#include "CRefTable.h"
#include "CPlugInList.h"
#include "CServerPlugin.h"
#include "SharedConsts.h"
#include "CPluginConfig.h"
#include <CoreFoundation/CFRunLoop.h>

#include <servers/bootstrap.h>
#include <time.h>

// --------------------------------------------------------------------------------
// * Globals
// --------------------------------------------------------------------------------


// --------------------------------------------------------------------------------
// * Externs
// --------------------------------------------------------------------------------
extern CFRunLoopRef		gServerRunLoop;

//--------------------------------------------------------------------------------------------------
// * CLauncher()
//
//--------------------------------------------------------------------------------------------------

CLauncher::CLauncher ( CServerPlugin *inPlugin )
	: DSCThread( kTSLauncherThread )
{
	fThreadSignature = kTSLauncherThread;

	if ( inPlugin == nil ) throw((sInt32)eParameterError);

	fPlugin = inPlugin;

} // CLauncher



//--------------------------------------------------------------------------------------------------
// * ~CLauncher()
//
//--------------------------------------------------------------------------------------------------

CLauncher::~CLauncher()
{
} // ~CLauncher



//--------------------------------------------------------------------------------------------------
// * StartThread()
//
//--------------------------------------------------------------------------------------------------

void CLauncher::StartThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	this->Resume();

	SetThreadRunState( kThreadRun );		// Tell our thread it's running

} // StartThread



//--------------------------------------------------------------------------------------------------
// * StopThread()
//
//--------------------------------------------------------------------------------------------------

void CLauncher::StopThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	// Check that the current thread context is not our thread context

	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


//--------------------------------------------------------------------------------------------------
// * ThreadMain()
//
//--------------------------------------------------------------------------------------------------

long CLauncher::ThreadMain ( void )
{
    			bool		done		= false;
				sInt32		siResult 	= eDSNoErr;
				uInt32		uiCntr		= 0;
				uInt32		uiAttempts	= 100;
    volatile	uInt32		uiWaitTime 	= 1;
				sHeader		aHeader;
				ePluginState		pluginState	= kUnknownState;
				CPluginConfig		pluginConfig;
 
	if ( gPlugins != nil )
	{
		pluginConfig.Initialize();

		while ( !done )
		{
			uiCntr++;

			// Attempt to initialize it
			siResult = fPlugin->Initialize();
			if ( ( siResult != eDSNoErr ) && ( uiCntr == 1 ) )
			{
				ERRORLOG3( kLogApplication, "Attempt #%l to initialize plug-in %s failed.\n  Will retry initialization at most 100 times every %l second.", uiCntr, fPlugin->GetPluginName(), uiWaitTime );
			}
			
			if ( siResult == eDSNoErr )
			{
				DBGLOG2( kLogApplication, "Initialization of plug-in %s succeeded with #%l attempt.", fPlugin->GetPluginName(), uiCntr );

				gPlugins->SetState( fPlugin->GetPluginName(), kInitalized );

				//provide the CFRunLoop to the plugins that need it
				if (gServerRunLoop != NULL)
				{
					aHeader.fType			= kServerRunLoop;
					aHeader.fResult			= eDSNoErr;
					aHeader.fContextData	= (void *)gServerRunLoop;
					siResult = fPlugin->ProcessRequest( (void*)&aHeader ); //don't handle return
				}

				pluginState = pluginConfig.GetPluginState( fPlugin->GetPluginName() );
				if ( pluginState == kInactive )
				{
					siResult = fPlugin->SetPluginState( kInactive );
					if ( siResult == eDSNoErr )
					{
						SRVRLOG1( kLogApplication, "Plug-in %s state is now inactive.", fPlugin->GetPluginName() );
				
						gPlugins->SetState( fPlugin->GetPluginName(), kInactive );
					}
					else
					{
						ERRORLOG2( kLogApplication, "Unable to set %s plug-in state to inactive.  Received error %l.", fPlugin->GetPluginName(), siResult );
					}
				}
				else
				{
				siResult = fPlugin->SetPluginState( kActive );
				if ( siResult == eDSNoErr )
				{
					SRVRLOG1( kLogApplication, "Plug-in %s state is now active.", fPlugin->GetPluginName() );

					gPlugins->SetState( fPlugin->GetPluginName(), kActive );
				}
				else
				{
					ERRORLOG2( kLogApplication, "Unable to set %s plug-in state to active.  Received error %l.", fPlugin->GetPluginName(), siResult );
				}
				}
				
				done = true;
			}

			if ( !done )
			{
				// We will try this 100 times before we bail
				if ( uiCntr == uiAttempts )
				{
					ERRORLOG2( kLogApplication, "%l attempts to initialize plug-in %s failed.\n  Setting plug-in state to inactive.", uiCntr, fPlugin->GetPluginName() );

					gPlugins->SetState( fPlugin->GetPluginName(), kInactive | kFailedToInit );

					siResult = fPlugin->SetPluginState( kInactive );

					done = true;
				}
				else
				{
					fWaitToInti.Wait( uiWaitTime * kMilliSecsPerSec );
				}
			}
		}
	}

	return( 0 );

} // ThreadMain


