/**
 * @file server/channel/src/StudioHandler.cpp
 * @ingroup channel
 *
 * @brief Token-auth HTTP API for /studio/health and /armory/stats.
 *
 * This file is part of the Channel Server (channel).
 *
 * Copyright (C) 2012-2020 COMP_hack Team <compomega@tutanota.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#include "StudioHandler.h"

// channel Includes
#include "ChannelServer.h"
#include "CharacterState.h"
#include "ChatManager.h"
#include "DemonState.h"
#include "TokuseiManager.h"

// Standard C++11 Includes
#include <cmath>

// object Includes
#include <Account.h>
#include <ChannelConfig.h>
#include <Character.h>
#include <Demon.h>

// libcomp Includes
#include <Log.h>

// Standard C++11 Includes
#include <cstring>
#include <string>

using namespace channel;

StudioHandler::StudioHandler(
    const std::shared_ptr<objects::ChannelConfig>& config,
    const std::shared_ptr<channel::ChannelServer>& server)
    : mConfig(config), mServer(server) {}

StudioHandler::~StudioHandler() {}

bool StudioHandler::Authorize(struct mg_connection* pConnection) const {
  if (!mConfig) {
    return false;
  }
  auto expected = mConfig->GetStudioToken();
  if (expected.IsEmpty()) {
    return false;
  }
  const char* token = mg_get_header(pConnection, "X-Studio-Token");
  if (!token) {
    return false;
  }
  return expected == libcomp::String(token);
}

void StudioHandler::ReplyJson(struct mg_connection* pConnection, int status,
                              const libcomp::String& jsonBody) const {
  const char* statusText = "OK";
  if (status == 400) {
    statusText = "Bad Request";
  } else if (status == 401) {
    statusText = "Unauthorized";
  } else if (status == 404) {
    statusText = "Not Found";
  } else if (status == 409) {
    statusText = "Conflict";
  } else if (status == 500) {
    statusText = "Internal Server Error";
  } else if (status == 503) {
    statusText = "Service Unavailable";
  }

  mg_printf(pConnection,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "Content-Length: %u\r\n"
            "\r\n"
            "%s",
            status, statusText, (unsigned)jsonBody.Length(),
            jsonBody.C());
}

bool StudioHandler::handleGet(CivetServer* pServer,
                              struct mg_connection* pConnection) {
  (void)pServer;

  if (!Authorize(pConnection)) {
    ReplyJson(pConnection, 401, "{\"ok\":false,\"error\":\"unauthorized\"}");
    return true;
  }

  const mg_request_info* info = mg_get_request_info(pConnection);
  if (!info) {
    return false;
  }

  libcomp::String uri(info->request_uri);
  libcomp::String path = uri.Contains("?") ? uri.LeftOf("?") : uri;
  if (path == "/armory/stats") {
    return HandleArmoryStats(pConnection, info);
  }
  if (path != "/studio/health") {
    return false;
  }

  ReplyJson(pConnection, 200, "{\"ok\":true}");
  return true;
}

libcomp::String StudioHandler::EscapeJson(const libcomp::String& s) {
  std::string escaped = s.ToUtf8();
  for (size_t i = 0; i < escaped.size(); ++i) {
    if (escaped[i] == '\\' || escaped[i] == '"') {
      escaped.insert(i, "\\");
      ++i;
    }
  }
  return libcomp::String(escaped);
}

bool StudioHandler::HandleArmoryStats(
    struct mg_connection* pConnection, const struct mg_request_info* info) {
  char nameBuf[33] = {};
  if (!info->query_string ||
      mg_get_var(info->query_string, strlen(info->query_string), "name",
                 nameBuf, sizeof(nameBuf)) <= 0 ||
      nameBuf[0] == '\0') {
    ReplyJson(pConnection, 400,
              "{\"ok\":false,\"error\":\"name query parameter required\"}");
    return true;
  }

  libcomp::String name(nameBuf);
  auto chat = mServer->GetChatManager();
  std::shared_ptr<objects::Character> targetCharacter;
  std::shared_ptr<objects::Account> targetAccount;
  std::shared_ptr<ChannelClientConnection> targetClient;
  if (!chat->GetTargetCharacterAccount(name, true, targetCharacter,
                                       targetAccount, targetClient) ||
      !targetClient) {
    ReplyJson(pConnection, 404,
              "{\"ok\":false,\"offline\":true,\"error\":\"character not "
              "online on this channel\"}");
    return true;
  }

  auto state = targetClient->GetClientState();
  auto cState = state ? state->GetCharacterState() : nullptr;
  if (!cState || !cState->Ready(true)) {
    ReplyJson(pConnection, 404,
              "{\"ok\":false,\"offline\":true,\"error\":\"character state "
              "unavailable\"}");
    return true;
  }

  auto definitionManager = mServer->GetDefinitionManager();
  cState->RecalculateStats(definitionManager);

  auto cs = cState->GetCoreStats();
  if (!cs) {
    ReplyJson(pConnection, 500,
              "{\"ok\":false,\"error\":\"missing core stats\"}");
    return true;
  }

  bool digitalized = cState->GetDigitalizeState() != nullptr;

  auto dState = state->GetDemonState();
  auto summonedDemon = dState ? dState->GetEntity() : nullptr;
  bool demonSummoned = summonedDemon != nullptr;

  libcomp::String json =
      libcomp::String("{\"ok\":true,\"source\":\"live\",\"online\":true,"
                      "\"digitalized\":%1,\"demonSummoned\":%2,\"name\":\"%3\"")
          .Arg(digitalized ? "true" : "false")
          .Arg(demonSummoned ? "true" : "false")
          .Arg(EscapeJson(name));

  if (demonSummoned) {
    auto dCore = summonedDemon->GetCoreStats().Get();
    json =
        json +
        libcomp::String(
            ",\"summonedDemon\":{\"id\":\"%1\",\"type\":%2,\"level\":%3}")
            .Arg(EscapeJson(summonedDemon->GetUUID().ToString()))
            .Arg(summonedDemon->GetType())
            .Arg(dCore ? (int32_t)dCore->GetLevel() : 0);
  }

  json = json + ",\"stats\":{";

  #define APPEND_STAT(key, value) \
    json = json + libcomp::String("\"%1\":%2,").Arg(key).Arg(value)

  APPEND_STAT("level", cs->GetLevel());
  APPEND_STAT("xp", static_cast<int32_t>(cs->GetXP()));
  APPEND_STAT("hp", cs->GetHP());
  APPEND_STAT("mp", cs->GetMP());
  APPEND_STAT("maxHp", cState->GetMaxHP());
  APPEND_STAT("maxMp", cState->GetMaxMP());
  APPEND_STAT("str", cState->GetSTR());
  APPEND_STAT("magic", cState->GetMAGIC());
  APPEND_STAT("vit", cState->GetVIT());
  APPEND_STAT("intel", cState->GetINTEL());
  APPEND_STAT("speed", cState->GetSPEED());
  APPEND_STAT("luck", cState->GetLUCK());
  APPEND_STAT("clsr", cState->GetCLSR());
  APPEND_STAT("lngr", cState->GetLNGR());
  APPEND_STAT("spell", cState->GetSPELL());
  APPEND_STAT("support", cState->GetSUPPORT());
  APPEND_STAT("pdef", cState->GetPDEF());
  json = json + libcomp::String("\"mdef\":%1},").Arg(cState->GetMDEF());

  #undef APPEND_STAT

  auto tokuseiManager = mServer->GetTokuseiManager();
  auto calcState = cState->GetCalculatedState();
  int32_t lbCapBonus = (int32_t)floor(tokuseiManager->GetAspectSum(
      cState, TokuseiAspectType::LIMIT_BREAK_MAX, calcState));

  json = json +
         libcomp::String(
             "\"combatBonuses\":{"
             "\"critical\":%1,"
             "\"finalCritChance\":%2,"
             "\"lbChance\":%3,"
             "\"lbDamage\":%4,"
             "\"lbCap\":%5,"
             "\"techAttackRate\":%6,"
             "\"techAttackPower\":%7,"
             "\"pursuitRate\":%8,"
             "\"pursuitPower\":%9,"
             "\"incant\":%10,"
             "\"cooldown\":%11"
             "}}")
             .Arg(cState->GetCorrectValue(CorrectTbl::CRITICAL))
             .Arg(cState->GetCorrectValue(CorrectTbl::FINAL_CRIT_CHANCE))
             .Arg(cState->GetCorrectValue(CorrectTbl::LB_CHANCE))
             .Arg(cState->GetCorrectValue(CorrectTbl::LB_DAMAGE))
             .Arg(30000 + lbCapBonus)
             .Arg((int32_t)floor(tokuseiManager->GetAspectSum(
                 cState, TokuseiAspectType::TECH_ATTACK_RATE, calcState)))
             .Arg((int32_t)floor(tokuseiManager->GetAspectSum(
                 cState, TokuseiAspectType::TECH_ATTACK_POWER, calcState)))
             .Arg((int32_t)floor(tokuseiManager->GetAspectSum(
                 cState, TokuseiAspectType::PURSUIT_RATE, calcState)))
             .Arg((int32_t)floor(tokuseiManager->GetAspectSum(
                 cState, TokuseiAspectType::PURSUIT_POWER, calcState)))
             .Arg(cState->GetCorrectValue(CorrectTbl::CHANT_TIME))
             .Arg(cState->GetCorrectValue(CorrectTbl::COOLDOWN_TIME));

  ReplyJson(pConnection, 200, json);
  return true;
}
