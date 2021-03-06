/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

//
// TrustStore.h - Abstract interface to permanent user trust assignments
//
#include <security_keychain/TrustStore.h>
#include <security_keychain/Globals.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/KCCursor.h>
#include <security_keychain/SecCFTypes.h>
#include <security_cdsa_utilities/Schema.h>


namespace Security {
namespace KeychainCore {


//
// Make and break: trivial
//
TrustStore::TrustStore(Allocator &alloc)
	: allocator(alloc), mRootsValid(false), mRootBytes(allocator)
{
}

TrustStore::~TrustStore()
{ }

//
// Retrieve the trust setting for a (certificate, policy) pair.
//
SecTrustUserSetting TrustStore::find(Certificate *cert, Policy *policy)
{
	if (Item item = findItem(cert, policy)) {
		// Make sure that the certificate is available in some keychain,
		// to provide a basis for editing the trust setting that we're returning.
		if (cert->keychain() == NULL) {
			StorageManager::KeychainList keychains;
			globals().storageManager.optionalSearchList((CFTypeRef)nil, keychains);
			if (cert->findInKeychain(keychains) == NULL) {
				Keychain defaultKeychain = Keychain::optional(NULL);
				if (Keychain location = item->keychain()) {
					try {
						cert->copyTo(location);	// add cert to the trust item's keychain
					} catch (...) {
						secdebug("trusteval", "failed to add certificate %p to keychain \"%s\"",
							cert, location->name());
						try {
							if (&*location != &*defaultKeychain)
								cert->copyTo(defaultKeychain);	// try the default (if it's not the same)
						} catch (...) {
							// unable to add the certificate
							secdebug("trusteval", "failed to add certificate %p to keychain \"%s\"",
								cert, defaultKeychain->name());
						}
					}
				}
			}
		}
		CssmDataContainer data;
		item->getData(data);
		if (data.length() != sizeof(TrustData))
			MacOSError::throwMe(errSecInvalidTrustSetting);
		TrustData &trust = *data.interpretedAs<TrustData>();
		if (trust.version != UserTrustItem::currentVersion)
			MacOSError::throwMe(errSecInvalidTrustSetting);
		return trust.trust;
	} else {
		return kSecTrustResultUnspecified;
	}
}


//
// Set an individual trust element
//
void TrustStore::assign(Certificate *cert, Policy *policy, SecTrustUserSetting trust)
{
	TrustData trustData = { UserTrustItem::currentVersion, trust };
	Keychain defaultKeychain = Keychain::optional(NULL);
	Keychain trustLocation = defaultKeychain;	// default keychain, unless trust entry found
	if (Item item = findItem(cert, policy)) {
		// user has a trust setting in a keychain - modify that
		trustLocation = item->keychain();
		if (trust == kSecTrustResultUnspecified)
			item->keychain()->deleteItem(item);
		else
			item->modifyContent(NULL, sizeof(trustData), &trustData);
	} else {
		// no trust entry: make one
		if (trust != kSecTrustResultUnspecified) {
			Item item = new UserTrustItem(cert, policy, trustData);
			if (Keychain location = cert->keychain()) {
				try {
					location->add(item);				// try the cert's keychain first
					trustLocation = location;
				} catch (...) {
					if (&*location != &*defaultKeychain)
						defaultKeychain->add(item);		// try the default (if it's not the same)
				}
			} else {
				defaultKeychain->add(item);				// raw cert - use default keychain
			}
		}
	}

	// Make sure that the certificate is available in some keychain,
	// to provide a basis for editing the trust setting that we're assigning.
	if (cert->keychain() == NULL) {
		StorageManager::KeychainList keychains;
		globals().storageManager.optionalSearchList((CFTypeRef)nil, keychains);
		if (cert->findInKeychain(keychains) == NULL) {
			try {
				cert->copyTo(trustLocation);	// add cert to the trust item's keychain
			} catch (...) {
				secdebug("trusteval", "failed to add certificate %p to keychain \"%s\"",
					cert, trustLocation->name());
				try {
					if (&*trustLocation != &*defaultKeychain)
						cert->copyTo(defaultKeychain);	// try the default (if it's not the same)
				} catch (...) {
					// unable to add the certificate
					secdebug("trusteval", "failed to add certificate %p to keychain \"%s\"",
						cert, defaultKeychain->name());
				}
			}
		}
	}
}


//
// Search the user's configured keychains for a trust setting.
// If found, return it (as a TrustItem). Otherwise, return NULL.
// Note that this function throws if a "real" error is encountered.
//
Item TrustStore::findItem(Certificate *cert, Policy *policy)
{
	try {
		SecKeychainAttribute attrs[2];
		CssmAutoData certIndex(Allocator::standard());
		UserTrustItem::makeCertIndex(cert, certIndex);
		attrs[0].tag = kSecTrustCertAttr;
		attrs[0].length = certIndex.length();
		attrs[0].data = certIndex.data();
		const CssmOid &policyOid = policy->oid();
		attrs[1].tag = kSecTrustPolicyAttr;
		attrs[1].length = policyOid.length();
		attrs[1].data = policyOid.data();
		SecKeychainAttributeList attrList = { 2, attrs };
		KCCursor cursor = globals().storageManager.createCursor(CSSM_DL_DB_RECORD_USER_TRUST, &attrList);
		Item item;
		if (cursor->next(item))
			return item;
		else
			return NULL;
	} catch (const CommonError &error) {
		return NULL;	// no trust schema, no records, no error
	}
}


//
// Return the root certificate list.
// This list is cached.
//
CFArrayRef TrustStore::copyRootCertificates()
{
	if (!mRootsValid) {
		loadRootCertificates();
		mCFRoots = NULL;
	}
	if (!mCFRoots) {
		uint32 count = mRoots.size();
		secdebug("anchors", "building %ld CF-style anchor certificates", count);
		vector<SecCertificateRef> roots(count);
        for (uint32 n = 0; n < count; n++) {
            SecPointer<Certificate> cert = new Certificate(mRoots[n],
                CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER);
            roots[n] = cert->handle();
        }
        mCFRoots = CFArrayCreate(NULL, (const void **)&roots[0], count,
            &kCFTypeArrayCallBacks);
        for (uint32 n = 0; n < count; n++)
            CFRelease(roots[n]);	// undo CFArray's retain
	}
    CFRetain(mCFRoots);
    return mCFRoots;
}

void TrustStore::getCssmRootCertificates(CertGroup &rootCerts)
{
	if (!mRootsValid)
		loadRootCertificates();
	rootCerts = CertGroup(CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER, CSSM_CERTGROUP_DATA);
	rootCerts.blobCerts() = &mRoots[0];
	rootCerts.count() = mRoots.size();
}

void TrustStore::refreshRootCertificates()
{
	if (mRootsValid) {
		secdebug("anchors", "clearing %ld cached anchor certificates", mRoots.size());
		
		// throw out the CF version
		if (mCFRoots) {
			CFRelease(mCFRoots);
			mCFRoots = NULL;
		}
		
		// release cert memory
		mRootBytes.reset();
		mRoots.clear();
		
		// all pristine again
		mRootsValid = false;
	}
}


//
// Load root (anchor) certificates from disk
//
void TrustStore::loadRootCertificates()
{
	using namespace CssmClient;
	using namespace KeychainCore::Schema;
	
	// release previous cached data (if any)
	refreshRootCertificates();
	
	static const char anchorLibrary[] = "/System/Library/Keychains/X509Anchors";

	// open anchor database and formulate query (x509v3 certs)
	secdebug("anchors", "Loading anchors from %s", anchorLibrary);
	DL dl(gGuidAppleFileDL);
	Db db(dl, anchorLibrary);
	DbCursor search(db);
	search->recordType(CSSM_DL_DB_RECORD_X509_CERTIFICATE);
	search->conjunctive(CSSM_DB_OR);
#if 0	// if we ever need to support v1/v2 certificates...
	search->add(CSSM_DB_EQUAL, kX509CertificateCertType, UInt32(CSSM_CERT_X_509v1));
	search->add(CSSM_DB_EQUAL, kX509CertificateCertType, UInt32(CSSM_CERT_X_509v2));
	search->add(CSSM_DB_EQUAL, kX509CertificateCertType, UInt32(CSSM_CERT_X_509v3));
#endif

	// collect certificate data
	typedef list<CssmDataContainer> ContainerList;
	ContainerList::iterator last;
	ContainerList certs;
	for (;;) {
		DbUniqueRecord id;
		last = certs.insert(certs.end(), CssmDataContainer());
		if (!search->next(NULL, &*last, id))
			break;
	}

	// how many data bytes do we need?
	size_t size = 0;
	for (ContainerList::const_iterator it = certs.begin(); it != last; it++)
		size += it->length();
	mRootBytes.length(size);

	// fill CssmData vector while copying data bytes together
	mRoots.clear();
	uint8 *base = mRootBytes.data<uint8>();
	for (ContainerList::const_iterator it = certs.begin(); it != last; it++) {
		memcpy(base, it->data(), it->length());
		mRoots.push_back(CssmData(base, it->length()));
		base += it->length();
	}
	secdebug("anchors", "%ld anchors loaded", mRoots.size());

	mRootsValid = true;			// ready to roll
}


} // end namespace KeychainCore
} // end namespace Security
