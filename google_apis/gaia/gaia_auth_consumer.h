// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_

#include <map>
#include <string>
#include <vector>

class GoogleServiceAuthError;

namespace net {
typedef std::vector<std::string> ResponseCookies;
}

typedef std::map<std::string, std::string> UserInfoMap;

// An interface that defines the callbacks for objects that
// GaiaAuthFetcher can return data to.
class GaiaAuthConsumer {
 public:
  struct ClientLoginResult {
    ClientLoginResult();
    ClientLoginResult(const std::string& new_sid,
                      const std::string& new_lsid,
                      const std::string& new_token,
                      const std::string& new_data);
    ClientLoginResult(const ClientLoginResult& other);
    ~ClientLoginResult();

    bool operator==(const ClientLoginResult &b) const;

    std::string sid;
    std::string lsid;
    std::string token;
    // TODO(chron): Remove the data field later. Don't use it if possible.
    std::string data;  // Full contents of ClientLogin return.
    bool two_factor;  // set to true if there was a TWO_FACTOR "failure".
  };

  struct ClientOAuthResult {
    ClientOAuthResult();
    ClientOAuthResult(const std::string& new_refresh_token,
                      const std::string& new_access_token,
                      int new_expires_in_secs);
    ~ClientOAuthResult();

    bool operator==(const ClientOAuthResult &b) const;

    // OAuth2 refresh token.  Used to mint new access tokens when needed.
    std::string refresh_token;

    // OAuth2 access token.  Token to pass to endpoints that require oauth2
    // authentication.
    std::string access_token;

    // The lifespan of |access_token| in seconds.
    int expires_in_secs;
  };

  virtual ~GaiaAuthConsumer() {}

  virtual void OnClientLoginSuccess(const ClientLoginResult& result) {}
  virtual void OnClientLoginFailure(const GoogleServiceAuthError& error) {}

  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token) {}
  virtual void OnIssueAuthTokenFailure(const std::string& service,
                                       const GoogleServiceAuthError& error) {}

  virtual void OnClientOAuthCode(const std::string& auth_code) {}
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) {}
  virtual void OnClientOAuthFailure(const GoogleServiceAuthError& error) {}

  virtual void OnOAuth2RevokeTokenCompleted() {}

  virtual void OnGetUserInfoSuccess(const UserInfoMap& data) {}
  virtual void OnGetUserInfoFailure(const GoogleServiceAuthError& error) {}

  virtual void OnUberAuthTokenSuccess(const std::string& token) {}
  virtual void OnUberAuthTokenFailure(const GoogleServiceAuthError& error) {}

  virtual void OnMergeSessionSuccess(const std::string& data) {}
  virtual void OnMergeSessionFailure(const GoogleServiceAuthError& error) {}

  virtual void OnListAccountsSuccess(const std::string& data) {}
  virtual void OnListAccountsFailure(const GoogleServiceAuthError& error) {}

  virtual void OnLogOutSuccess() {}
  virtual void OnLogOutFailure(const GoogleServiceAuthError& error) {}

  virtual void OnGetCheckConnectionInfoSuccess(const std::string& data) {}
  virtual void OnGetCheckConnectionInfoError(
      const GoogleServiceAuthError& error) {}
};

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_CONSUMER_H_
