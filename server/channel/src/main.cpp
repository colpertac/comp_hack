/**
 * @file server/channel/src/main.cpp
 * @ingroup channel
 *
 * @author HACKfrost
 *
 * @brief Main channel server file.
 *
 * This file is part of the Channel Server (channel).
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

// channel Includes
#include "ChannelConfig.h"
#include "ChannelServer.h"
#include "StudioHandler.h"

// libcomp Includes
#include <Config.h>
#include <Constants.h>
#include <Exception.h>
#include <Log.h>
#include <PersistentObject.h>
#include <ServerCommandLineParser.h>
#include <Shutdown.h>

// civetweb Includes
#include <CivetServer.h>

#if defined(_WIN32) && defined(WIN32_SERV)
int ApplicationMain(int argc, const char *argv[])
#else
int main(int argc, const char *argv[])
#endif  // defined(_WIN32) && defined(WIN32_SERV)
{
  libcomp::Exception::RegisterSignalHandler();

  libcomp::Log::GetSingletonPtr()->AddStandardOutputHook();

  libcomp::Config::LogVersion("COMP_hack Channel Server");

  std::string configPath =
      libcomp::BaseServer::GetDefaultConfigPath() + "channel.xml";

  // Command line argument parser.
  auto parser = std::make_shared<libcomp::ServerCommandLineParser>();

  // Parse the command line arguments.
  if (!parser->Parse(argc, argv)) {
    return EXIT_FAILURE;
  }

  auto arguments = parser->GetStandardArguments();

  if (!arguments.empty()) {
    configPath = arguments.front().ToUtf8();

    LogGeneralDebug([&]() {
      return libcomp::String("Using custom config path %1\n").Arg(configPath);
    });

    size_t pos = configPath.find_last_of("\\/");
    if (std::string::npos != pos) {
      libcomp::BaseServer::SetConfigPath(
          configPath.substr(0, ((size_t)pos + 1)));
    }
  }

  auto config = std::make_shared<objects::ChannelConfig>();
  if (!libcomp::BaseServer::ReadConfig(config, configPath)) {
    LogGeneralWarningMsg(
        "Failed to load the channel config file. Default values will be "
        "used.\n");
  }

  if (!libcomp::PersistentObject::Initialize()) {
    LogGeneralCriticalMsg(
        "One or more persistent object definition failed to load.\n");

    return EXIT_FAILURE;
  }

  auto server =
      std::make_shared<channel::ChannelServer>(argv[0], config, parser);

  if (!server->Initialize()) {
    LogGeneralCriticalMsg("The server could not be initialized.\n");

    return EXIT_FAILURE;
  }

  // Optional studio HTTP for portrait worker (0 = disabled).
  // Binds 0.0.0.0 so homelab/LAN can reach it; auth is StudioToken.
  CivetServer* pStudioServer = nullptr;
  channel::StudioHandler* pStudioHandler = nullptr;
  {
    auto chConfig = std::dynamic_pointer_cast<objects::ChannelConfig>(config);
    uint16_t studioPort = chConfig ? chConfig->GetStudioHttpPort() : 0;
    if (studioPort != 0) {
      if (chConfig->GetStudioToken().IsEmpty()) {
        LogGeneralWarningMsg(
            "StudioHttpPort set but StudioToken empty — studio API disabled.\n");
      } else {
        std::vector<std::string> options;
        options.push_back("listening_ports");
        options.push_back(std::string("0.0.0.0:") + std::to_string(studioPort));
        try {
          pStudioHandler = new channel::StudioHandler(chConfig, server);
          pStudioServer = new CivetServer(options);
          pStudioServer->addHandler("/studio/health", pStudioHandler);
          pStudioServer->addHandler("/studio/dress", pStudioHandler);
          pStudioServer->addHandler("/studio/pose", pStudioHandler);
          pStudioServer->addHandler("/studio/nameplate", pStudioHandler);
          pStudioServer->addHandler("/studio/ensure-name", pStudioHandler);
          pStudioServer->addHandler("/armory/stats", pStudioHandler);
          LogGeneralInfo([&]() {
            return libcomp::String(
                       "Studio API listening on 0.0.0.0:%1 "
                       "(/studio/health, /studio/dress, /studio/pose, "
                       "/studio/nameplate, /studio/ensure-name, "
                       "/armory/stats)\n")
                .Arg(studioPort);
          });
        } catch (const CivetException& e) {
          LogGeneralError([&]() {
            return libcomp::String("Failed to start studio HTTP: %1\n")
                .Arg(e.what());
          });
          delete pStudioServer;
          pStudioServer = nullptr;
          delete pStudioHandler;
          pStudioHandler = nullptr;
        }
      }
    }
  }

  // Set this for the signal handler.
  libcomp::Shutdown::Configure(server.get());

  // Start the main server loop (blocks until done).
  int returnCode = server->Start(true);

  delete pStudioServer;
  pStudioServer = nullptr;
  delete pStudioHandler;
  pStudioHandler = nullptr;

  // Complete the shutdown process.
  libcomp::Shutdown::Complete();

  LogGeneralInfoMsg("Bye!\n");

  // Stop the logger
  delete libcomp::Log::GetSingletonPtr();

  return returnCode;
}
