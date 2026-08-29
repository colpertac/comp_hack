/**
 * @file server/channel/src/StudioHandler.cpp
 * @ingroup channel
 *
 * @brief HTTP API for portrait studio mannequin dress/pose (token-auth).
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
#include "ManagerConnection.h"
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

// JsonBox Includes
#include <JsonBox.h>

// Standard C++11 Includes
#include <cstring>
#include <string>

using namespace channel;

namespace {

libcomp::String JsonGetString(const JsonBox::Object& obj, const char* key) {
  auto it = obj.find(key);
  if (it == obj.end() || !it->second.isString()) {
    return "";
  }
  return libcomp::String(it->second.getString());
}

bool JsonGetNumber(const JsonBox::Object& obj, const char* key, double& out) {
  auto it = obj.find(key);
  if (it == obj.end() || !it->second.isNumeric()) {
    return false;
  }
  out = static_cast<double>(it->second.getInteger());
  return true;
}

}  // namespace

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

  auto chat = mServer->GetChatManager();
  auto onlineAccount = [&](const char* username) -> bool {
    auto cl =
        mServer->GetManagerConnection()->GetClientConnection(username);
    if (!cl) {
      return false;
    }
    auto st = cl->GetClientState();
    auto cState = st ? st->GetCharacterState() : nullptr;
    return cState && cState->GetEntity();
  };
  auto checkOnlineChar = [&](const char* name) -> bool {
    libcomp::String n(name);
    std::shared_ptr<objects::Character> ch;
    std::shared_ptr<objects::Account> ac;
    std::shared_ptr<ChannelClientConnection> cl;
    if (!chat->GetTargetCharacterAccount(n, true, ch, ac, cl)) {
      return false;
    }
    return cl != nullptr;
  };

  // Prefer account username (vam1/vaf1). Character name usually matches.
  bool vam1 = onlineAccount("vam1") || checkOnlineChar("vam1") ||
              onlineAccount("vam") || checkOnlineChar("vam");
  bool vaf1 = onlineAccount("vaf1") || checkOnlineChar("vaf1") ||
              onlineAccount("vaf") || checkOnlineChar("vaf");
  bool va = onlineAccount("va") || checkOnlineChar("va");

  ReplyJson(pConnection, 200,
            libcomp::String("{\"ok\":true,\"vam1\":%1,\"vaf1\":%2,\"vam\":%3,"
                            "\"vaf\":%4,\"va\":%5}")
                .Arg(vam1 ? "true" : "false")
                .Arg(vaf1 ? "true" : "false")
                .Arg(vam1 ? "true" : "false")
                .Arg(vaf1 ? "true" : "false")
                .Arg(va ? "true" : "false"));
  return true;
}

bool StudioHandler::handlePost(CivetServer* pServer,
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
  if (uri == "/studio/dress") {
    return HandleDress(pConnection);
  }
  if (uri == "/studio/pose") {
    return HandlePose(pConnection);
  }
  if (uri == "/studio/nameplate") {
    return HandleNameplate(pConnection);
  }
  if (uri == "/studio/ensure-name") {
    return HandleEnsureName(pConnection);
  }
  return false;
}

bool StudioHandler::ReadJsonBody(struct mg_connection* pConnection,
                                 JsonBox::Object& outObj) {
  const mg_request_info* info = mg_get_request_info(pConnection);
  if (!info) {
    return false;
  }

  size_t len = static_cast<size_t>(info->content_length);
  if (len == 0 || len > 65536) {
    ReplyJson(pConnection, 411, "{\"ok\":false,\"error\":\"length\"}");
    return false;
  }

  std::string body(len, '\0');
  size_t read = 0;
  while (read < len) {
    int n = mg_read(pConnection, &body[read], len - read);
    if (n <= 0) {
      break;
    }
    read += static_cast<size_t>(n);
  }
  if (read != len) {
    ReplyJson(pConnection, 400, "{\"ok\":false,\"error\":\"body\"}");
    return false;
  }

  JsonBox::Value root;
  root.loadFromString(body);
  if (!root.isObject()) {
    ReplyJson(pConnection, 400, "{\"ok\":false,\"error\":\"json\"}");
    return false;
  }
  outObj = root.getObject();
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

bool StudioHandler::HandleDress(struct mg_connection* pConnection) {
  JsonBox::Object obj;
  if (!ReadJsonBody(pConnection, obj)) {
    return true;
  }

  auto mannequin = JsonGetString(obj, "mannequin");
  auto source = JsonGetString(obj, "source");
  if (mannequin.IsEmpty() || source.IsEmpty()) {
    ReplyJson(pConnection, 400,
              "{\"ok\":false,\"error\":\"need mannequin and source\"}");
    return true;
  }

  bool doPose = true;
  auto poseIt = obj.find("pose");
  if (poseIt != obj.end() && poseIt->second.isBoolean()) {
    doPose = poseIt->second.getBoolean();
  }

  // Default visible (admin dress). Worker passes plate:false for shots.
  bool showPlate = true;
  auto plateIt = obj.find("plate");
  if (plateIt != obj.end() && plateIt->second.isBoolean()) {
    showPlate = plateIt->second.getBoolean();
  }

  uint32_t zoneID = 10105;
  // Park mannequins apart so male/female don't stack in the same void.
  // male (vam1/vam/va): +50000,+50000 — female (vaf1/vaf): -50000,-50000
  float x = 50000.f;
  float y = 50000.f;
  {
    libcomp::String mannequinLower = mannequin.ToLower();
    if (mannequinLower == "vaf" || mannequinLower == "vaf1") {
      x = -50000.f;
      y = -50000.f;
    }
  }
  double num = 0;
  if (JsonGetNumber(obj, "zone", num)) {
    zoneID = static_cast<uint32_t>(num);
  }
  if (JsonGetNumber(obj, "x", num)) {
    x = static_cast<float>(num);
  }
  if (JsonGetNumber(obj, "y", num)) {
    y = static_cast<float>(num);
  }

  auto chat = mServer->GetChatManager();
  std::shared_ptr<ChannelClientConnection> client;
  auto resolveErr = chat->ResolveStudioMannequinClient(mannequin, client);
  if (!resolveErr.IsEmpty() || !client) {
    ReplyJson(pConnection, 409,
              libcomp::String("{\"ok\":false,\"error\":\"%1\"}")
                  .Arg(EscapeJson(resolveErr.IsEmpty()
                                      ? libcomp::String("mannequin offline: %1")
                                            .Arg(mannequin)
                                      : resolveErr)));
    return true;
  }

  auto err = chat->DressStudioMannequin(client, source, true);
  if (!err.IsEmpty()) {
    ReplyJson(pConnection, 409,
              libcomp::String("{\"ok\":false,\"error\":\"%1\"}")
                  .Arg(EscapeJson(err)));
    return true;
  }

  if (doPose) {
    auto poseErr = chat->PoseStudioMannequin(client, zoneID, x, y);
    if (!poseErr.IsEmpty()) {
      ReplyJson(pConnection, 409,
                libcomp::String("{\"ok\":false,\"error\":\"%1\",\"dressed\":"
                                "true}")
                    .Arg(EscapeJson(poseErr)));
      return true;
    }
  }

  if (!showPlate) {
    auto plateErr = chat->HideStudioNameplate(client);
    if (!plateErr.IsEmpty()) {
      ReplyJson(pConnection, 409,
                libcomp::String("{\"ok\":false,\"error\":\"%1\",\"dressed\":"
                                "true,\"posed\":%2}")
                    .Arg(EscapeJson(plateErr))
                    .Arg(doPose ? "true" : "false"));
      return true;
    }
  }

  LogGeneralInfo([&]() {
    return libcomp::String("Studio dress: %1 ← %2 (pose=%3 plate=%4)\n")
        .Arg(mannequin)
        .Arg(source)
        .Arg(doPose ? "yes" : "no")
        .Arg(showPlate ? "yes" : "no");
  });

  ReplyJson(pConnection, 200,
            libcomp::String("{\"ok\":true,\"mannequin\":\"%1\",\"source\":"
                            "\"%2\",\"posed\":%3,\"plate\":%4}")
                .Arg(mannequin)
                .Arg(source)
                .Arg(doPose ? "true" : "false")
                .Arg(showPlate ? "true" : "false"));
  return true;
}

bool StudioHandler::HandlePose(struct mg_connection* pConnection) {
  JsonBox::Object obj;
  if (!ReadJsonBody(pConnection, obj)) {
    return true;
  }

  auto mannequin = JsonGetString(obj, "mannequin");
  if (mannequin.IsEmpty()) {
    ReplyJson(pConnection, 400,
              "{\"ok\":false,\"error\":\"need mannequin\"}");
    return true;
  }

  uint32_t zoneID = 10105;
  float x = 50000.f;
  float y = 50000.f;
  {
    libcomp::String mannequinLower = mannequin.ToLower();
    if (mannequinLower == "vaf" || mannequinLower == "vaf1") {
      x = -50000.f;
      y = -50000.f;
    }
  }
  double num = 0;
  if (JsonGetNumber(obj, "zone", num)) {
    zoneID = static_cast<uint32_t>(num);
  }
  if (JsonGetNumber(obj, "x", num)) {
    x = static_cast<float>(num);
  }
  if (JsonGetNumber(obj, "y", num)) {
    y = static_cast<float>(num);
  }

  auto chat = mServer->GetChatManager();
  std::shared_ptr<ChannelClientConnection> client;
  auto resolveErr = chat->ResolveStudioMannequinClient(mannequin, client);
  if (!resolveErr.IsEmpty() || !client) {
    ReplyJson(pConnection, 409,
              libcomp::String("{\"ok\":false,\"error\":\"%1\"}")
                  .Arg(EscapeJson(resolveErr.IsEmpty()
                                      ? libcomp::String("mannequin offline: %1")
                                            .Arg(mannequin)
                                      : resolveErr)));
    return true;
  }

  auto poseErr = chat->PoseStudioMannequin(client, zoneID, x, y);
  if (!poseErr.IsEmpty()) {
    ReplyJson(pConnection, 409,
              libcomp::String("{\"ok\":false,\"error\":\"%1\"}")
                  .Arg(EscapeJson(poseErr)));
    return true;
  }

  LogGeneralInfo([&]() {
    return libcomp::String("Studio pose: %1 zone=%2 (%3,%4)\n")
        .Arg(mannequin)
        .Arg(zoneID)
        .Arg(x)
        .Arg(y);
  });

  ReplyJson(pConnection, 200,
            libcomp::String("{\"ok\":true,\"mannequin\":\"%1\",\"posed\":true,"
                            "\"zone\":%2}")
                .Arg(mannequin)
                .Arg(zoneID));
  return true;
}

bool StudioHandler::HandleNameplate(struct mg_connection* pConnection) {
  JsonBox::Object obj;
  if (!ReadJsonBody(pConnection, obj)) {
    return true;
  }

  auto mannequin = JsonGetString(obj, "mannequin");
  if (mannequin.IsEmpty()) {
    ReplyJson(pConnection, 400,
              "{\"ok\":false,\"error\":\"need mannequin\"}");
    return true;
  }

  bool visible = true;
  auto visIt = obj.find("visible");
  if (visIt != obj.end() && visIt->second.isBoolean()) {
    visible = visIt->second.getBoolean();
  }

  auto chat = mServer->GetChatManager();
  std::shared_ptr<ChannelClientConnection> client;
  auto resolveErr = chat->ResolveStudioMannequinClient(mannequin, client);
  if (!resolveErr.IsEmpty() || !client) {
    ReplyJson(pConnection, 409,
              libcomp::String("{\"ok\":false,\"error\":\"%1\"}")
                  .Arg(EscapeJson(resolveErr.IsEmpty()
                                      ? libcomp::String("mannequin offline: %1")
                                            .Arg(mannequin)
                                      : resolveErr)));
    return true;
  }

  libcomp::String err =
      visible ? chat->RestoreStudioNameplate(client)
              : chat->HideStudioNameplate(client);
  if (!err.IsEmpty()) {
    ReplyJson(pConnection, 409,
              libcomp::String("{\"ok\":false,\"error\":\"%1\"}")
                  .Arg(EscapeJson(err)));
    return true;
  }

  LogGeneralInfo([&]() {
    return libcomp::String("Studio nameplate: %1 visible=%2\n")
        .Arg(mannequin)
        .Arg(visible ? "yes" : "no");
  });

  ReplyJson(pConnection, 200,
            libcomp::String("{\"ok\":true,\"mannequin\":\"%1\",\"visible\":%2}")
                .Arg(mannequin)
                .Arg(visible ? "true" : "false"));
  return true;
}

bool StudioHandler::HandleEnsureName(struct mg_connection* pConnection) {
  JsonBox::Object obj;
  if (!ReadJsonBody(pConnection, obj)) {
    return true;
  }

  auto mannequin = JsonGetString(obj, "mannequin");
  if (mannequin.IsEmpty()) {
    ReplyJson(pConnection, 400,
              "{\"ok\":false,\"error\":\"need mannequin\"}");
    return true;
  }

  auto chat = mServer->GetChatManager();
  bool repaired = false;
  libcomp::String name;
  auto err = chat->EnsureStudioMannequinName(mannequin, repaired, name);
  if (!err.IsEmpty()) {
    ReplyJson(pConnection, 409,
              libcomp::String("{\"ok\":false,\"error\":\"%1\"}")
                  .Arg(EscapeJson(err)));
    return true;
  }

  ReplyJson(pConnection, 200,
            libcomp::String("{\"ok\":true,\"mannequin\":\"%1\",\"name\":\"%2\","
                            "\"repaired\":%3}")
                .Arg(mannequin)
                .Arg(EscapeJson(name))
                .Arg(repaired ? "true" : "false"));
  return true;
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
