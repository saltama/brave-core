// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

#include "brave/browser/brave_rewards/add_funds_popup.h"

#include <string>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "brave/components/brave_rewards/browser/rewards_service.h"
#include "brave/components/brave_shields/common/brave_shield_constants.h"
#include "brave/components/toolbar/constants.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "net/base/escape.h"
#include "third_party/blink/public/platform/web_referrer_policy.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace {

  constexpr int kPopupPreferredHeight = 800;
  constexpr int kPopupPreferredWidth = 1100;

  // Referrer.
  const char kRewardsHost[] = "rewards";

#define TESTING 0

  // URL to open in the popup.
#if TESTING
  const char kAddFundsUrl[] = "http://localhost:8000/uphold/index.html";
#else
  const char kAddFundsUrl[] = "https://uphold-widget-uhocggaamg.now.sh/index.html";
#endif

  // Content permission URLs.
#if TESTING
  const char kBraveHost[] = "http://localhost:8000";
#else
  const char kBraveHost[] = "https://uphold-widget-uhocggaamg.now.sh";
#endif
  const char kUpholdHost[] = "https://sandbox.uphold.com";
  const char kFirstParty[] = "https://firstParty";

  const std::map<std::string, std::string> kCurrencyToNetworkMap{
      {"BTC", "bitcoin"},
      {"BAT", "ethereum"},
      {"ETH", "ethereum"},
      {"LTC", "litecoin"} };

} // namepsace

namespace brave_rewards {

AddFundsPopup::AddFundsPopup() : add_funds_popup_(nullptr), profile_(nullptr) {}

AddFundsPopup::~AddFundsPopup() {
  ClosePopup();
}

// Show existing or open a new popup.
void AddFundsPopup::ShowPopup(content::WebContents* initiator,
  RewardsService* rewards_service) {
  add_funds_popup_
    ? add_funds_popup_->Focus()
    : OpenPopup(initiator, rewards_service);
}

void AddFundsPopup::OpenPopup(content::WebContents* initiator,
  RewardsService* rewards_service) {
  DCHECK(!add_funds_popup_);
  DCHECK(initiator);
  DCHECK(rewards_service);
  if (!initiator || !rewards_service)
    return;

  const std::map<std::string, std::string> addresses = 
    rewards_service->GetAddresses();
  if (addresses.empty())
    return; 

  content::WebContentsDelegate* wc_delegate = initiator->GetDelegate();
  if (!wc_delegate)
    return;

  const GURL gurl(kAddFundsUrl);
  const GURL referrer_gurl(std::string(brave_toolbar::kInternalUIScheme) +
    kRewardsHost);
  const content::Referrer referrer(referrer_gurl,
    blink::kWebReferrerPolicyAlways);
  content::OpenURLParams params(gurl, referrer,
    WindowOpenDisposition::NEW_POPUP,
    ui::PAGE_TRANSITION_LINK, true);

  // Supply addresses via post data. The data is currently in the query string
  // format (application/x-www-form-urlencoded):
  // addresses=UrlEscapedBase64EncodedStringifiedJSON.
  params.uses_post = true;
  const std::string data = ToQueryString(GetAddressesAsJSON(addresses));
  params.post_data =
    network::ResourceRequestBody::CreateFromBytes(data.data(), data.size());
  params.extra_headers =
    std::string("Content-Type: application/x-www-form-urlencoded\r\n") +
    "Content-Length: " + std::to_string(data.size()) + "\r\n\r\n";

  // Open the popup.
  add_funds_popup_ = wc_delegate->OpenURLFromTab(initiator, params);
  DCHECK(add_funds_popup_);
  if (!add_funds_popup_)
    return;

  views::Widget* topLevelWidget = views::Widget::GetTopLevelWidgetForNativeView(
    add_funds_popup_->GetNativeView());
  if (topLevelWidget) {
    // We need to know when the popup closes.
    topLevelWidget->AddObserver(this);
    // Reposition/resize the new popup.
    gfx::Rect popup_bounds = CalculatePopupWindowBounds(initiator);
    topLevelWidget->SetBounds(popup_bounds);
    // Let Uphold content bypass shields, use camera and autoplay. Only do this
    // if we added an observer, otherwise we won't be able to reset when the
    // popup closes.
    RelaxContentPermissions();
  }
}

void AddFundsPopup::ClosePopup() {
  if (!add_funds_popup_)
    return;

   views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
    add_funds_popup_->GetNativeView());
   if (widget)
    widget->RemoveObserver(this);

  add_funds_popup_->ClosePage();
  add_funds_popup_ = nullptr;
}

// content::WidgetObserver implementation.
void AddFundsPopup::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  ResetContentPermissions();
  add_funds_popup_ = nullptr;
}

std::string AddFundsPopup::GetAddressesAsJSON(
    const std::map<std::string, std::string>& addresses) {
  // Create a dictionary of addresses for serialization.
  auto addresses_dictionary = std::make_unique<base::DictionaryValue>();
  for (const auto& pair : addresses) {
    auto address = std::make_unique<base::DictionaryValue>();
    address->SetString("address", pair.second);
    address->SetString("currency", pair.first);
    DCHECK(kCurrencyToNetworkMap.count(pair.first));
    address->SetString("network", kCurrencyToNetworkMap.at(pair.first));
    addresses_dictionary->SetDictionary(pair.first, std::move(address));
  }

  std::string data;
  base::JSONWriter::Write(*addresses_dictionary, &data);
  return data;
}

std::string AddFundsPopup::ToQueryString(const std::string& data) {
  std::string query_string_value;
  base::Base64Encode(data, &query_string_value);
  return ("addresses=" + net::EscapeUrlEncodedData(query_string_value, false));
}

gfx::Rect AddFundsPopup::CalculatePopupWindowBounds(WebContents* initiator) {
  // Get bounds of the initiator content to see if they would fit the
  // preferred size of our popup.
  WebContents* outermost_web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(initiator);
  gfx::Rect initiator_bounds = outermost_web_contents->GetContainerBounds();

  gfx::Point center = initiator_bounds.CenterPoint();
  gfx::Rect popup_bounds(center.x() - kPopupPreferredWidth / 2,
                         center.y() - kPopupPreferredHeight / 2,
                         kPopupPreferredWidth, kPopupPreferredHeight);
  // Popup fits within the initiator, return the bounds no matter where the
  // initiator is on the screen.
  if (initiator_bounds.Contains(popup_bounds))
    return popup_bounds;

  // Move the popup to the center of the screen if it ended up off screen.
  // If the initiator is split between multiple displays this will show the
  // popup on the display that contains the largest chunk of the initiator
  // window. If the popup is too big for the screen, shrink it to fit.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(
          outermost_web_contents->GetNativeView());
  if (!display.bounds().IsEmpty() && !display.bounds().Contains(popup_bounds)) {
    popup_bounds = display.bounds();
    popup_bounds.ClampToCenteredSize(
        gfx::Size(kPopupPreferredWidth, kPopupPreferredHeight));
  }

  return popup_bounds;
}

void AddFundsPopup::RelaxContentPermissions() {
  // Get contents settings map for the current profile.
  profile_ = Profile::FromBrowserContext(add_funds_popup_->GetBrowserContext());
  DCHECK(profile_ && !profile_->IsOffTheRecord());
  HostContentSettingsMap* map =
    HostContentSettingsMapFactory::GetForProfile(profile_);

  fingerprinting_setting_ =
    DisableShieldsFingerprinting(map, kBraveHost, nullptr);
  fingerprinting_setting_fp_ =
    DisableShieldsFingerprinting(map, kBraveHost, kFirstParty);
  camera_setting_ = AllowCameraAccess(map, kUpholdHost);
  autoplay_setting_ = AllowAutoplay(map, kUpholdHost);
}

ContentSetting AddFundsPopup::DisableShieldsFingerprinting(
  HostContentSettingsMap* map,
  const char* host,
  const char* secondary) {
  return SetContentSetting(map, host, secondary, CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTING_ALLOW,
    brave_shields::kFingerprinting);
}

ContentSetting AddFundsPopup::AllowCameraAccess(HostContentSettingsMap* map,
  const char* host) {
  return SetContentSetting(map, host, nullptr,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
    CONTENT_SETTING_ALLOW, std::string());
}

ContentSetting AddFundsPopup::AllowAutoplay(HostContentSettingsMap* map,
  const char* host) {
  return SetContentSetting(map, host, nullptr, CONTENT_SETTINGS_TYPE_AUTOPLAY,
    CONTENT_SETTING_ALLOW, std::string());
}

ContentSetting AddFundsPopup::SetContentSetting(HostContentSettingsMap* map,
  const char* host,
  const char* secondary,
  ContentSettingsType type,
  ContentSetting setting,
  const std::string& resource_identifier) {
  DCHECK(map);
  DCHECK(host);

  const GURL gurl(host);
  GURL gurl_secondary;
  if (secondary) {
    gurl_secondary = GURL(secondary);
  }

  // Get the current setting.
  ContentSetting current_setting =
    map->GetContentSetting(gurl, gurl_secondary, type, resource_identifier);

  // Check if the setting is already want we want it to be.
  if (current_setting != setting) {
    // For PLUGINS type, construct patterns and use custom scope.
    if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
      const ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(std::string(host) + "/*");
      ContentSettingsPattern pattern_secondary =
          ContentSettingsPattern::Wildcard();
      if (secondary)
        pattern_secondary =
            ContentSettingsPattern::FromString(std::string(secondary) + "/*");

      map->SetContentSettingCustomScope(pattern, pattern_secondary, type,
        resource_identifier, setting);
    } else {
      // For other types use default scope.
      ContentSetting default_setting =
        map->GetDefaultContentSetting(type, nullptr);
      if (current_setting == default_setting)
        current_setting = CONTENT_SETTING_DEFAULT;
      map->SetContentSettingDefaultScope(gurl, gurl_secondary, type,
        resource_identifier, setting);
    }
  }

  return current_setting;
}

void AddFundsPopup::ResetContentPermissions() {
  // Get contents settings map for the current profile.
  DCHECK(profile_);
  HostContentSettingsMap* map =
    HostContentSettingsMapFactory::GetForProfile(profile_);

  ResetShieldsFingerprinting(map, kBraveHost, nullptr, fingerprinting_setting_);
  ResetShieldsFingerprinting(map, kBraveHost, kFirstParty,
    fingerprinting_setting_fp_);
  ResetCameraAccess(map, kUpholdHost);
  ResetAutoplay(map, kUpholdHost);
}

void AddFundsPopup::ResetShieldsFingerprinting(HostContentSettingsMap* map,
                                               const char* host,
                                               const char* secondary,
                                               ContentSetting setting) {
  SetContentSetting(map, host, secondary, CONTENT_SETTINGS_TYPE_PLUGINS,
                    setting, brave_shields::kFingerprinting);
}

void AddFundsPopup::ResetCameraAccess(HostContentSettingsMap* map,
  const char* host) {
  ResetContentSetting(map, kUpholdHost,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
    camera_setting_);
}

void AddFundsPopup::ResetAutoplay(HostContentSettingsMap* map,
  const char* host) {
  ResetContentSetting(map, kUpholdHost, CONTENT_SETTINGS_TYPE_AUTOPLAY,
    autoplay_setting_);
}

void AddFundsPopup::ResetContentSetting(HostContentSettingsMap* map,
  const char* host,
  ContentSettingsType type,
  ContentSetting setting) {
  DCHECK(map);
  DCHECK(host);
  DCHECK(type != CONTENT_SETTINGS_TYPE_PLUGINS);

  const GURL gurl(host);
  if (setting == CONTENT_SETTING_DEFAULT ||
      setting != map->GetContentSetting(gurl, GURL(), type, std::string())) {
    map->SetContentSettingDefaultScope(gurl, GURL(), type, std::string(),
                                       setting);
  }
}

}  // namespace brave_rewards
