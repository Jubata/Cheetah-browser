// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "jni/CertificateChainHelper_jni.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

static ScopedJavaLocalRef<jobjectArray>
JNI_CertificateChainHelper_GetCertificateChain(
    JNIEnv* env,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jobjectArray>();

  scoped_refptr<net::X509Certificate> cert =
      web_contents->GetController().GetVisibleEntry()->GetSSL().certificate;
  if (!cert)
    return ScopedJavaLocalRef<jobjectArray>();

  std::vector<std::string> cert_chain;
  cert_chain.reserve(1 + cert->GetIntermediateCertificates().size());
  cert_chain.emplace_back(
      net::x509_util::CryptoBufferAsStringPiece(cert->os_cert_handle()));
  for (auto* handle : cert->GetIntermediateCertificates())
    cert_chain.emplace_back(net::x509_util::CryptoBufferAsStringPiece(handle));

  return base::android::ToJavaArrayOfByteArray(env, cert_chain);
}
