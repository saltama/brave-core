/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_rewards/browser/extension_rewards_service_observer.h"

#include "brave/common/extensions/api/brave_rewards.h"
#include "brave/components/brave_rewards/browser/rewards_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/event_router.h"

namespace brave_rewards {

ExtensionRewardsServiceObserver::ExtensionRewardsServiceObserver(
    Profile* profile)
    : profile_(profile) {
}

ExtensionRewardsServiceObserver::~ExtensionRewardsServiceObserver() {
}

void ExtensionRewardsServiceObserver::OnWalletInitialized(
    RewardsService* rewards_service,
    int error_code) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  if (event_router) {
    std::unique_ptr<base::ListValue> args(new base::ListValue());
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::BRAVE_WALLET_CREATED,
        extensions::api::brave_rewards::OnWalletCreated::kEventName,
        std::move(args)));
    event_router->BroadcastEvent(std::move(event));
  }
}

void ExtensionRewardsServiceObserver::OnWalletProperties(
    RewardsService* rewards_service,
    int error_code,
    std::unique_ptr<brave_rewards::WalletProperties> wallet_properties) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  if (event_router && wallet_properties) {
    extensions::api::brave_rewards::OnWalletProperties::Properties properties;

    properties.probi = wallet_properties->probi;
    properties.balance = wallet_properties->balance;
    properties.rates.btc = wallet_properties->rates["BTC"];
    properties.rates.eth = wallet_properties->rates["ETH"];
    properties.rates.usd = wallet_properties->rates["USD"];
    properties.rates.eur = wallet_properties->rates["EUR"];

    for (size_t i = 0; i < wallet_properties->grants.size(); i ++) {
      properties.grants.push_back(
          extensions::api::brave_rewards::OnWalletProperties::Properties::
              GrantsType());
      auto& grant = properties.grants[properties.grants.size() -1];

      grant.altcurrency = wallet_properties->grants[i].altcurrency;
      grant.probi = wallet_properties->grants[i].probi;
      grant.expiry_time = wallet_properties->grants[i].expiryTime;
    }

    std::unique_ptr<base::ListValue> args(
        extensions::api::brave_rewards::OnWalletProperties::Create(properties)
            .release());

    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::BRAVE_ON_WALLET_PROPERTIES,
        extensions::api::brave_rewards::OnWalletProperties::kEventName,
        std::move(args)));
    event_router->BroadcastEvent(std::move(event));
  }
}

}  // namespace brave_rewards
