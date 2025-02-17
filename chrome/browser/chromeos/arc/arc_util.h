// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_UTIL_H_

#include <stdint.h>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/callback_forward.h"

// Most utility should be put in components/arc/arc_util.{h,cc}, rather than
// here. However, some utility implementation requires other modules defined in
// chrome/, so this file contains such utilities.
// Note that it is not allowed to have dependency from components/ to chrome/
// by DEPS.

class AccountId;
class Profile;

namespace base {
class FilePath;
}

namespace arc {

// Values to be stored in the local state preference to keep track of the
// filesystem encryption migration status.
enum FileSystemCompatibilityState : int32_t {
  // No migration has happened, user keeps using the old file system.
  kFileSystemIncompatible = 0,
  // Migration has happened. New filesystem is in use.
  kFileSystemCompatible = 1,
  // Migration has happened, and a notification about it was already shown.
  kFileSystemCompatibleAndNotified = 2,

  // Existing code assumes that kFileSystemIncompatible is the only state
  // representing incompatibility and other values are all variants of
  // "compatible" state. Be careful in the case adding a new enum value.
};

// Returns true if ARC is allowed to run for the given profile.
// Otherwise, returns false, e.g. if the Profile is not for the primary user,
// ARC is not available on the device, it is in the flow to set up managed
// account creation.
// nullptr can be safely passed to this function. In that case, returns false.
bool IsArcAllowedForProfile(const Profile* profile);

// Returns true if the profile is unmanaged or if the policy
// EcryptfsMigrationStrategy for the user doesn't disable the migration.
// Specifically if the policy states to ask the user, it is also considered that
// migration is allowed, so return true.
bool IsArcMigrationAllowedByPolicyForProfile(const Profile* profile);

// Returns true if the profile is temporarily blocked to run ARC in the current
// session, because the filesystem storing the profile is incompatible with the
// currently installed ARC version.
//
// The actual filesystem check is performed only when it is running on the
// Chrome OS device. Otherwise, it just returns the dummy value set by
// SetArcBlockedDueToIncompatibleFileSystemForTesting (false by default.)
bool IsArcBlockedDueToIncompatibleFileSystem(const Profile* profile);

// Sets the result of IsArcBlockedDueToIncompatibleFileSystem for testing.
void SetArcBlockedDueToIncompatibleFileSystemForTesting(bool block);

// Returns true if the profile is already marked to be on a filesystem
// compatible to the currently installed ARC version. The check almost never
// is meaningful on test workstation. Usually it should be checked only when
// running on the real Chrome OS.
bool IsArcCompatibleFileSystemUsedForProfile(const Profile* profile);

// Disallows ARC for all profiles for testing.
// In most cases, disabling ARC should be done via commandline. However,
// there are some cases to be tested where ARC is available, but ARC is not
// supported for some reasons (e.g. incognito mode, supervised user,
// secondary profile). On the other hand, some test infra does not support
// such situations (e.g. API test). This is for workaround to emulate the
// case.
void DisallowArcForTesting();

// Returns whether the user has opted in (or is opting in now) to use Google
// Play Store on ARC.
// This is almost equivalent to the value of "arc.enabled" preference. However,
// in addition, if ARC is not allowed for the given |profile|, then returns
// false. Please see detailed condition for the comment of
// IsArcAllowedForProfile().
// Note: For historical reason, the preference name is not matched with the
// actual meaning.
bool IsArcPlayStoreEnabledForProfile(const Profile* profile);

// Returns whether the preference "arc.enabled" is managed or not.
// It is requirement for a caller to ensure ARC is allowed for the user of
// the given |profile|.
bool IsArcPlayStoreEnabledPreferenceManagedForProfile(const Profile* profile);

// Enables or disables Google Play Store on ARC. Currently, it is tied to
// ARC enabled state, too, so this also should trigger to enable or disable
// whole ARC system.
// If the preference is managed, then no-op.
// It is requirement for a caller to ensure ARC is allowed for the user of
// the given |profile|.
// TODO(hidehiko): De-couple the concept to enable ARC system and opt-in
// to use Google Play Store. Note that there is a plan to use ARC without
// Google Play Store, then ARC can run without opt-in. Returns false in case
// enabled state of the Play Store cannot be changed.
bool SetArcPlayStoreEnabledForProfile(Profile* profile, bool enabled);

// Returns whether all ARC related OptIn preferences (i.e.
// ArcBackupRestoreEnabled and ArcLocationServiceEnabled) are managed or unused
// (e.g. for Active Directory users).
bool AreArcAllOptInPreferencesIgnorableForProfile(const Profile* profile);

// Returns true iff there is a user associated with |profile|, and it is an
// Active Directory user.
bool IsActiveDirectoryUserForProfile(const Profile* profile);

// Returns true if ChromeOS OOBE opt-in window is currently showing.
bool IsArcOobeOptInActive();

// Checks and updates the preference value whether the underlying filesystem
// for the profile is compatible with ARC, when necessary. After it's done (or
// skipped), |callback| is run either synchronously or asynchronously.
void UpdateArcFileSystemCompatibilityPrefIfNeeded(
    const AccountId& account_id,
    const base::FilePath& profile_path,
    const base::Closure& callback);

// Returns whether Google Assistant feature is allowed for given |profile|.
ash::mojom::AssistantAllowedState IsAssistantAllowedForProfile(
    const Profile* profile);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_UTIL_H_
