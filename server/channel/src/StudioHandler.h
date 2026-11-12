/**
 * @file server/channel/src/StudioHandler.h
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

#ifndef SERVER_CHANNEL_SRC_STUDIOHANDLER_H
#define SERVER_CHANNEL_SRC_STUDIOHANDLER_H

// civetweb Includes
#include <CivetServer.h>

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
 * Civet handler for GET /studio/health (status probe) and GET /armory/stats
 * (live character stats). Requires X-Studio-Token matching ChannelConfig.
 */
class StudioHandler : public CivetHandler {
 public:
  StudioHandler(const std::shared_ptr<objects::ChannelConfig>& config,
                const std::shared_ptr<channel::ChannelServer>& server);

  virtual ~StudioHandler();

  virtual bool handleGet(CivetServer* pServer, struct mg_connection* pConnection);

 private:
  bool Authorize(struct mg_connection* pConnection) const;

  void ReplyJson(struct mg_connection* pConnection, int status,
                 const libcomp::String& jsonBody) const;

  bool HandleArmoryStats(struct mg_connection* pConnection,
                         const struct mg_request_info* info);

  static libcomp::String EscapeJson(const libcomp::String& s);

  std::shared_ptr<objects::ChannelConfig> mConfig;
  std::shared_ptr<channel::ChannelServer> mServer;
};

}  // namespace channel

#endif  // SERVER_CHANNEL_SRC_STUDIOHANDLER_H
