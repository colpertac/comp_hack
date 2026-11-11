/**
 * @file server/lobby/src/packets/internal/CharacterLogin.cpp
 * @ingroup lobby
 *
 * @brief Apply world→lobby character presence (channel/zone/status/position).
 *
 * This file is part of the Lobby Server (lobby).
 *
 * Copyright (C) 2012-2020 COMP_hack Team <compomega@tutanota.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Packets.h"

// libcomp Includes
#include <Log.h>
#include <ManagerPacket.h>
#include <Packet.h>
#include <PacketCodes.h>
#include <ReadOnlyPacket.h>

// lobby Includes
#include "AccountManager.h"
#include "LobbyServer.h"

using namespace lobby;

bool Parsers::CharacterLogin::Parse(
    libcomp::ManagerPacket* pPacketManager,
    const std::shared_ptr<libcomp::TcpConnection>& connection,
    libcomp::ReadOnlyPacket& p) const {
  (void)connection;

  // World → lobby presence snapshot:
  //   username (String16 UTF-8), channelID (s8), zoneID (u32), status (s8)
  //   optional: x (float), y (float)
  if (p.Left() < 2) {
    LogGeneralErrorMsg("Invalid CharacterLogin presence packet from world.\n");
    return false;
  }

  libcomp::String username = p.ReadString16Little(
      libcomp::Convert::Encoding_t::ENCODING_UTF8, true);
  if (username.IsEmpty() || p.Left() < 6) {
    LogGeneralErrorMsg(
        "CharacterLogin presence packet missing channel/zone/status.\n");
    return false;
  }

  int8_t channelID = p.ReadS8();
  uint32_t zoneID = p.ReadU32Little();
  int8_t status = p.ReadS8();

  float x = 0.f;
  float y = 0.f;
  bool hasPos = false;
  if (p.Left() >= 8) {
    x = p.ReadFloat();
    y = p.ReadFloat();
    hasPos = true;
  }

  auto server =
      std::dynamic_pointer_cast<LobbyServer>(pPacketManager->GetServer());
  if (!server->GetAccountManager()->UpdateCharacterPresence(
          username, channelID, zoneID, status, x, y, hasPos)) {
    LogGeneralDebug([&]() {
      return libcomp::String(
                 "Ignored CharacterLogin presence for offline/unknown "
                 "account: %1\n")
          .Arg(username);
    });
  }

  return true;
}
