/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  MscACL.h
 *  TokendMuscle
 */

#ifndef _MSCACL_H_
#define _MSCACL_H_

#include <PCSC/musclecard.h>

#if 0
class MscACL
{
public:
	typedef struct
	{
		MSCUShort16 readPermission;
		MSCUShort16 writePermission;
		MSCUShort16 usePermission;
	}
	MSCKeyACL, *MSCLPKeyACL;

	typedef struct
	{
		MSCUShort16 readPermission;
		MSCUShort16 writePermission;
		MSCUShort16 deletePermission;
	}
	MSCObjectACL, *MSCLPObjectACL, MSCCertACL, *MSCLPCertACL;
};
#endif

#endif /* !_MSCACL_H_ */

/* arch-tag: 952A79D0-BE68-11D8-BF9F-000A95C4302E */
