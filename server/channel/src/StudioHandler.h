/**
 * @file server/channel/src/StudioHandler.h
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

#ifndef SERVER_CHANNEL_SRC_STUDIOHANDLER_H
#define SERVER_CHANNEL_SRC_STUDIOHANDLER_H

// civetweb Includes
#include <CivetServer.h>

// JsonBox Includes
#include <JsonBox.h>

// libcomp Includes
#include <CString.h>

// Standard C++11 Includes
#include <memory>

namespace objects {
class ChannelConfig;
}

namespace channel {

class ChannelServer;

/**
 * Loopback Civet handler for /studio/health, /studio/dress, /studio/pose,
 * /studio/nameplate, /studio/ensure-name.
 * Bind only to 127.0.0.1; require X-Studio-Token matching ChannelConfig.
 */
class StudioHandler : public CivetHandler {
 public:
  StudioHandler(const std::shared_ptr<objects::ChannelConfig>& config,
                const std::shared_ptr<channel::ChannelServer>& server);

  virtual ~StudioHandler();

  virtual bool handleGet(CivetServer* pServer, struct mg_connection* pConnection);

  virtual bool handlePost(CivetServer* pServer,
                          struct mg_connection* pConnection);

 private:
  bool Authorize(struct mg_connection* pConnection) const;

  void ReplyJson(struct mg_connection* pConnection, int status,
                 const libcomp::String& jsonBody) const;

  bool ReadJsonBody(struct mg_connection* pConnection, JsonBox::Object& outObj);

  bool HandleDress(struct mg_connection* pConnection);

  bool HandlePose(struct mg_connection* pConnection);

  bool HandleNameplate(struct mg_connection* pConnection);

  bool HandleEnsureName(struct mg_connection* pConnection);

  bool HandleArmoryStats(struct mg_connection* pConnection,
                         const struct mg_request_info* info);

  static libcomp::String EscapeJson(const libcomp::String& s);

  std::shared_ptr<objects::ChannelConfig> mConfig;
  std::shared_ptr<channel::ChannelServer> mServer;
};

}  // namespace channel

#endif  // SERVER_CHANNEL_SRC_STUDIOHANDLER_H
