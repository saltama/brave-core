// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef BRAVE_BROWSER_REWARDS_ADD_FUNDS_POPUP_H_
#define BRAVE_BROWSER_REWARDS_ADD_FUNDS_POPUP_H_

#include <map>
#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}

class HostContentSettingsMap;
class Profile;

namespace views {
class Widget;
}

namespace brave_rewards {

class RewardsService;

class AddFundsPopup  : public views::WidgetObserver {
public:
  AddFundsPopup();
  ~AddFundsPopup() override;

  // content::WidgetObserver implementation.
  void OnWidgetClosing(views::Widget* widget) override;

  // Show existing or open a new popup.
  void ShowPopup(content::WebContents* initiator,
    RewardsService* rewards_service);

private:
  // Popup management.
  void OpenPopup(content::WebContents* initiator,
    RewardsService* rewards_service);
  void ClosePopup();
  gfx::Rect CalculatePopupWindowBounds(content::WebContents* initiator);
  
  // Addreses handling.
  std::string GetAddressesAsJSON(
    const std::map<std::string, std::string>& addresses);
  std::string ToQueryString(const std::string& data);

  // Content permissions.
  void RelaxContentPermissions();
  ContentSetting DisableShieldsFingerprinting(HostContentSettingsMap* map,
                                              const char* host,
                                              const char* secondary);
  ContentSetting AllowCameraAccess(HostContentSettingsMap* map,
                                   const char* host);
  ContentSetting AllowAutoplay(HostContentSettingsMap* map, const char* host);
  
  ContentSetting SetContentSetting(HostContentSettingsMap* map,
    const char* host,
    const char* secondary,
    ContentSettingsType type,
    ContentSetting setting,
    const std::string& resource_identifier);

  void ResetContentPermissions();
  void ResetShieldsFingerprinting(HostContentSettingsMap* map,
                                  const char* host,
                                  const char* secondary,
                                  ContentSetting setting);
  void ResetCameraAccess(HostContentSettingsMap* map, const char* host);
  void ResetAutoplay(HostContentSettingsMap* map, const char* host);
  void ResetContentSetting(HostContentSettingsMap* map,
    const char* host,
    ContentSettingsType type,
    ContentSetting setting);

  // Popup content handle.
  content::WebContents* add_funds_popup_;

  // Profile for which content setting have been altered.
  Profile* profile_;

  // Content settings.
  ContentSetting fingerprinting_setting_;
  ContentSetting fingerprinting_setting_fp_;
  ContentSetting camera_setting_;
  ContentSetting autoplay_setting_;

  DISALLOW_COPY_AND_ASSIGN(AddFundsPopup);
};

} // namespace brav_rewards

#endif  // BRAVE_BROWSER_REWARDS_ADD_FUNDS_POPUP_H_
