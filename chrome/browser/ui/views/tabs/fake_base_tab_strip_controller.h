// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/base/models/list_selection_model.h"

class TabStripImpl;

class FakeBaseTabStripController : public TabStripController {
 public:
  FakeBaseTabStripController();
  ~FakeBaseTabStripController() override;

  void AddTab(int index, bool is_active);
  void AddPinnedTab(int index, bool is_active);
  void RemoveTab(int index);

  ui::ListSelectionModel* selection_model() { return &selection_model_; }

  void set_tab_strip(TabStripImpl* tab_strip) { tab_strip_ = tab_strip; }

  // TabStripController overrides:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  int GetCount() const override;
  bool IsValidIndex(int index) const override;
  bool IsActiveTab(int index) const override;
  int GetActiveIndex() const override;
  bool IsTabSelected(int index) const override;
  bool IsTabPinned(int index) const override;
  void SelectTab(int index) override;
  void ExtendSelectionTo(int index) override;
  void ToggleSelected(int index) override;
  void AddSelectionFromAnchorTo(int index) override;
  void CloseTab(int index, CloseTabSource source) override;
  void ToggleTabAudioMute(int index) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  void UpdateLoadingAnimations() override;
  int HasAvailableDragActions() const override;
  void OnDropIndexUpdate(int index, bool drop_before) override;
  void PerformDrop(bool drop_before, int index, const GURL& url) override;
  bool IsCompatibleWith(TabStripImpl* other) const override;
  void CreateNewTab() override;
  void CreateNewTabWithLocation(const base::string16& loc) override;
  bool IsIncognito() override;
  void StackedLayoutMaybeChanged() override;
  void OnStartedDraggingTabs() override;
  void OnStoppedDraggingTabs() override;
  void CheckFileSupported(const GURL& url) override;
  SkColor GetToolbarTopSeparatorColor() const override;
  base::string16 GetAccessibleTabName(const Tab* tab) const override;
  Profile* GetProfile() const override;

 private:
  TabStripImpl* tab_strip_ = nullptr;

  int num_tabs_ = 0;
  int active_index_ = -1;

  ui::ListSelectionModel selection_model_;

  DISALLOW_COPY_AND_ASSIGN(FakeBaseTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
