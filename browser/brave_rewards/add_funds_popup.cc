// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

#include "brave/browser/brave_rewards/add_funds_popup.h"

#include <string>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "brave/components/toolbar/constants.h"
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

const char kAddFundsUrl[] = "https://uphold-widget-uhocggaamg.now.sh/index.html";
const char kRewardsHost[] = "rewards";

const std::map<std::string, std::string> kCurrencyToNetworkMap{
    {"BTC", "bitcoin"},
    {"BAT", "ethereum"},
    {"ETH", "ethereum"},
    {"LTC", "litecoin"}};

std::string GetAddressesAsJSON(
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

std::string ToQueryString(const std::string& data) {
  std::string query_string_value;
  base::Base64Encode(data, &query_string_value);
  return ("addresses=" + net::EscapeUrlEncodedData(query_string_value, false));
}

gfx::Rect CalculatePopupWindowBounds(WebContents* initiator) {
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

}  // namespace

namespace brave_rewards {

// Open Add Funds as a popup window.
WebContents* OpenAddFundsWindow(
    WebContents* initiator,
    const std::map<std::string, std::string>& addresses) {
  DCHECK(initiator);
  DCHECK(!addresses.empty());

  if (!initiator)
    return nullptr;

  content::WebContentsDelegate* wc_delegate = initiator->GetDelegate();
  if (!wc_delegate)
    return nullptr;

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

  WebContents* newWebContents = wc_delegate->OpenURLFromTab(initiator, params);
  if (!newWebContents)
    return nullptr;

  // Reposition/resize the new popup.
  gfx::Rect popup_bounds = CalculatePopupWindowBounds(initiator);
  views::Widget* topLevelWidget = views::Widget::GetTopLevelWidgetForNativeView(
      newWebContents->GetNativeView());
  if (topLevelWidget)
    topLevelWidget->SetBounds(popup_bounds);

  return newWebContents;
}

}  // namespace brave_rewards
