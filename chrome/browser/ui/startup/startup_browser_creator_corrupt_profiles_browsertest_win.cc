// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"

namespace {

void UnblockOnProfileCreation(Profile::CreateStatus expected_final_status,
                              const base::Closure& quit_closure,
                              Profile* profile,
                              Profile::CreateStatus status) {
  // If the status is CREATE_STATUS_CREATED, then the function will be called
  // again with CREATE_STATUS_INITIALIZED.
  if (status == Profile::CREATE_STATUS_CREATED)
    return;

  EXPECT_EQ(expected_final_status, status);
  quit_closure.Run();
}

void UnblockOnProfileInitialized(const base::Closure& quit_closure,
                                 Profile* profile,
                                 Profile::CreateStatus status) {
  UnblockOnProfileCreation(Profile::CREATE_STATUS_INITIALIZED, quit_closure,
                           profile, status);
}

void OnCloseAllBrowsersSucceeded(const base::Closure& quit_closure,
                                 const base::FilePath& path) {
  quit_closure.Run();
}

void CreateAndSwitchToProfile(const std::string& basepath) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  base::FilePath path = profile_manager->user_data_dir().AppendASCII(basepath);
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      path, base::Bind(&UnblockOnProfileInitialized, run_loop.QuitClosure()),
      base::string16(), std::string(), std::string());
  // Run the message loop to allow profile creation to take place; the loop is
  // terminated by UnblockOnProfileCreation when the profile is created.
  run_loop.Run();

  profiles::SwitchToProfile(path, false, ProfileManager::CreateCallback(),
                            ProfileMetrics::SWITCH_PROFILE_ICON);
}

void CheckBrowserWindows(const std::vector<std::string>& expected_basepaths) {
  std::vector<std::string> actual_basepaths;
  for (const Browser* browser : *BrowserList::GetInstance()) {
    actual_basepaths.push_back(
        browser->profile()->GetPath().BaseName().AsUTF8Unsafe());
  }

  if (actual_basepaths.size() != expected_basepaths.size() ||
      !std::is_permutation(actual_basepaths.cbegin(), actual_basepaths.cend(),
                           expected_basepaths.cbegin())) {
    ADD_FAILURE()
        << "Expected profile paths are different from actual profile paths."
           "\n  Actual profile paths: "
        << base::JoinString(actual_basepaths, ", ")
        << "\n  Expected profile paths: "
        << base::JoinString(expected_basepaths, ", ");
  }
}

void ExpectUserManagerToShow() {
  // If the user manager is not shown yet, wait for the user manager to appear.
  if (!UserManager::IsShowing()) {
    base::RunLoop run_loop;
    UserManager::AddOnUserManagerShownCallbackForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }
  ASSERT_TRUE(UserManager::IsShowing());

  // We must hide the user manager before the test ends.
  UserManager::Hide();
}

}  // namespace

class StartupBrowserCreatorCorruptProfileTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorCorruptProfileTest()
      : test_body_has_run_(false), expect_test_body_to_run_(true) {}

  void SetExpectTestBodyToRun(bool expected_result) {
    expect_test_body_to_run_ = expected_result;
  }

  bool DeleteProfileData(const std::string& basepath) {
    base::FilePath user_data_dir;
    if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      return false;

    base::FilePath dir_to_delete = user_data_dir.AppendASCII(basepath);
    return base::DirectoryExists(dir_to_delete) &&
           base::DeleteFile(dir_to_delete, true);
  }

  bool RemoveCreateDirectoryPermissionForUserDataDirectory() {
    base::FilePath user_data_dir;
    return PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) &&
           base::DenyFilePermission(user_data_dir, FILE_ADD_SUBDIRECTORY);
  }

 protected:
  // For each test, declare a bool SetUpUserDataDirectory[testname] function.
  bool SetUpUserDataDirectoryForLastOpenedProfileMissing();
  bool SetUpUserDataDirectoryForLastUsedProfileFallbackToLastOpenedProfiles();
  bool SetUpUserDataDirectoryForLastUsedProfileFallbackToUserManager();
  bool SetUpUserDataDirectoryForCannotCreateSystemProfile();
  bool SetUpUserDataDirectoryForLastUsedProfileFallbackToAnyProfile();
  bool SetUpUserDataDirectoryForLastUsedProfileFallbackFail();
  bool SetUpUserDataDirectoryForDoNotStartLockedProfile();
  bool SetUpUserDataDirectoryForNoFallbackForUserSelectedProfile();
  bool SetUpUserDataDirectoryForDeletedProfileFallbackToUserManager();

  void CloseBrowsersSynchronouslyForProfileBasePath(
           const std::string& basepath) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ASSERT_TRUE(profile_manager);

    Profile* profile =
        profile_manager->GetProfileByPath(
            profile_manager->user_data_dir().AppendASCII(basepath));
    ASSERT_TRUE(profile);

    base::RunLoop run_loop;
    BrowserList::GetInstance()->CloseAllBrowsersWithProfile(
        profile,
        base::Bind(&OnCloseAllBrowsersSucceeded, run_loop.QuitClosure()),
        BrowserList::CloseCallback(), false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kRestoreLastSession);
    command_line->AppendSwitch(switches::kNoErrorDialogs);
  }

  // In this test fixture, SetUpUserDataDirectory must be handled for all
  // non-PRE_ tests.
  bool SetUpUserDataDirectory() override {
#define SET_UP_USER_DATA_DIRECTORY_FOR(testname) \
if (testing::UnitTest::GetInstance()->current_test_info()->name() == \
    std::string(#testname)) {\
  return this->SetUpUserDataDirectoryFor ## testname(); \
}

    SET_UP_USER_DATA_DIRECTORY_FOR(LastOpenedProfileMissing);
    SET_UP_USER_DATA_DIRECTORY_FOR(LastUsedProfileFallbackToLastOpenedProfiles);
    SET_UP_USER_DATA_DIRECTORY_FOR(LastUsedProfileFallbackToUserManager);
    SET_UP_USER_DATA_DIRECTORY_FOR(CannotCreateSystemProfile);
    SET_UP_USER_DATA_DIRECTORY_FOR(LastUsedProfileFallbackToAnyProfile);
    SET_UP_USER_DATA_DIRECTORY_FOR(LastUsedProfileFallbackFail);
    SET_UP_USER_DATA_DIRECTORY_FOR(DoNotStartLockedProfile);
    SET_UP_USER_DATA_DIRECTORY_FOR(NoFallbackForUserSelectedProfile);
    SET_UP_USER_DATA_DIRECTORY_FOR(DeletedProfileFallbackToUserManager);

#undef SET_UP_USER_DATA_DIRECTORY_FOR

    // If control goes here, it means SetUpUserDataDirectory is not handled.
    // This is okay for PRE_ tests, but not acceptable for main tests.
    if (base::StartsWith(
            testing::UnitTest::GetInstance()->current_test_info()->name(),
             "PRE_", base::CompareCase::SENSITIVE)) {
      return true;
    }

    ADD_FAILURE() << "SetUpUserDataDirectory is not handled by the test.";
    return false;
  }

  void TearDownOnMainThread() override {
    test_body_has_run_ = true;
  }

  void TearDown() override {
    EXPECT_EQ(expect_test_body_to_run_, test_body_has_run_);
    InProcessBrowserTest::TearDown();
  }

 private:
  bool test_body_has_run_;
  bool expect_test_body_to_run_;
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreatorCorruptProfileTest);
};

// Most of the tests below have three sections:
// (1) PRE_ test, which is used to create profiles. Most of these profiles are
//     meant to be opened in (3) during startup.
// (2) StartupBrowserCreatorCorruptProfileTest::SetUpUserDataDirectoryFor...
//     which sets up the user data directory, i.e. to remove profile directories
//     and to prevent the profile directories from being created again in (3).
//     We cannot remove these directories while the browser process is running,
//     so that cannot be done during (1) or (3).
// (3) The test itself. With the |kRestoreLastSession| switch set, profiles
//     opened in (1) are reopened. However, since the directories of some
//     profiles are removed in (2), these profiles are unable to be initialized.
//     We are testing whether the correct fallback action is taken.

// LastOpenedProfileMissing : If any last opened profile is missing, that
// profile is skipped, but other last opened profiles should be opened in the
// browser.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_LastOpenedProfileMissing) {
  CreateAndSwitchToProfile("Profile 1");
  CreateAndSwitchToProfile("Profile 2");
}

bool StartupBrowserCreatorCorruptProfileTest::
         SetUpUserDataDirectoryForLastOpenedProfileMissing() {
  return DeleteProfileData("Profile 1") &&
         RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       LastOpenedProfileMissing) {
  CheckBrowserWindows({"Default", "Profile 2"});
}

// LastUsedProfileFallbackToLastOpenedProfiles : If the last used profile is
// missing, it should fall back to any last opened profiles.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_LastUsedProfileFallbackToLastOpenedProfiles) {
  CreateAndSwitchToProfile("Profile 1");
  CreateAndSwitchToProfile("Profile 2");
}

bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForLastUsedProfileFallbackToLastOpenedProfiles() {
  return DeleteProfileData("Profile 2") &&
         RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       LastUsedProfileFallbackToLastOpenedProfiles) {
  CheckBrowserWindows({"Default", "Profile 1"});
}

// LastUsedProfileFallbackToUserManager : If all last opened profiles are
// missing, it should fall back to user manager. To open the user manager, both
// the guest profile and the system profile must be creatable.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_LastUsedProfileFallbackToUserManager) {
  CreateAndSwitchToProfile("Profile 1");
  CloseBrowsersSynchronouslyForProfileBasePath("Profile 1");
  CreateAndSwitchToProfile("Profile 2");

  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::CreateDirectory(ProfileManager::GetGuestProfilePath()));
  ASSERT_TRUE(base::CreateDirectory(ProfileManager::GetSystemProfilePath()));
}

bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForLastUsedProfileFallbackToUserManager() {
  return DeleteProfileData("Default") &&
         DeleteProfileData("Profile 2") &&
         RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       LastUsedProfileFallbackToUserManager) {
  CheckBrowserWindows({});
  ExpectUserManagerToShow();
}


// CannotCreateSystemProfile : If the system profile cannot be created, the user
// manager should not be shown. Fallback to any other profile.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_CannotCreateSystemProfile) {
  CreateAndSwitchToProfile("Profile 1");
  CloseBrowsersSynchronouslyForProfileBasePath("Profile 1");
  CreateAndSwitchToProfile("Profile 2");

  // Create the guest profile path, but not the system profile one. This will
  // make it impossible to create the system profile once the permissions are
  // locked down during setup.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::CreateDirectory(ProfileManager::GetGuestProfilePath()));
}

bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForCannotCreateSystemProfile() {
  return DeleteProfileData("Default") &&
         DeleteProfileData("Profile 2") &&
         RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       CannotCreateSystemProfile) {
  CheckBrowserWindows({"Profile 1"});
}

// LastUsedProfileFallbackToAnyProfile : If all the last opened profiles and the
// guest profile cannot be opened, fall back to any profile that is not locked.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_LastUsedProfileFallbackToAnyProfile) {
  CreateAndSwitchToProfile("Profile 1");
  CloseBrowsersSynchronouslyForProfileBasePath("Profile 1");
  CreateAndSwitchToProfile("Profile 2");
}

bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForLastUsedProfileFallbackToAnyProfile() {
  return DeleteProfileData("Default") &&
         DeleteProfileData("Profile 2") &&
         RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       LastUsedProfileFallbackToAnyProfile) {
  CheckBrowserWindows({"Profile 1"});
}

// LastUsedProfileFallbackFail : If no startup option is feasible, the browser
// should quit cleanly.
bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForLastUsedProfileFallbackFail() {
  SetExpectTestBodyToRun(false);
  return RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       LastUsedProfileFallbackFail) {
  ADD_FAILURE() << "Test body is not expected to run.";
}

// DoNotStartLockedProfile : Profiles that are locked should never be
// initialized. Since there are no unlocked profiles, the browser should not
// start.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_DoNotStartLockedProfile) {
  // Lock the default profile. The user manager is shown after the profile is
  // locked.
  profiles::LockProfile(browser()->profile());
  ExpectUserManagerToShow();
}

bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForDoNotStartLockedProfile() {
  SetExpectTestBodyToRun(false);
  return RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       DoNotStartLockedProfile) {
  ADD_FAILURE() << "Test body is not expected to run.";
}

// NoFallbackForUserSelectedProfile : No fallback should be attempted if the
// profile is selected by the --profile-directory switch. The browser should not
// start if the specified profile could not be initialized.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_NoFallbackForUserSelectedProfile) {
  CreateAndSwitchToProfile("Profile 1");
  CreateAndSwitchToProfile("Profile 2");
}

bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForNoFallbackForUserSelectedProfile() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kProfileDirectory, "Profile 1");
  SetExpectTestBodyToRun(false);
  return DeleteProfileData("Profile 1") &&
         RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       NoFallbackForUserSelectedProfile) {
  ADD_FAILURE() << "Test body is not expected to run.";
}

// DeletedProfileFallbackToUserManager : If the profile's entry in
// ProfileAttributesStorage is missing, it means the profile is deleted. The
// browser should not attempt to open the profile, but should show the user
// manager instead.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       PRE_DeletedProfileFallbackToUserManager) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::CreateDirectory(ProfileManager::GetGuestProfilePath()));
  ASSERT_TRUE(base::CreateDirectory(ProfileManager::GetSystemProfilePath()));
}

bool StartupBrowserCreatorCorruptProfileTest::
    SetUpUserDataDirectoryForDeletedProfileFallbackToUserManager() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Simulate a deleted profile by not creating the profile at the first place.
  command_line->AppendSwitchASCII(switches::kProfileDirectory, "Not Found");
  return RemoveCreateDirectoryPermissionForUserDataDirectory();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorCorruptProfileTest,
                       DeletedProfileFallbackToUserManager) {
  CheckBrowserWindows({});
  ExpectUserManagerToShow();
}
