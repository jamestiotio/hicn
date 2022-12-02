/*
 * Copyright (c) 2021-2022 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <core/global_configuration.h>
#include <hicn/transport/core/io_module.h>
#include <hicn/transport/core/prefix.h>

#include <libconfig.h++>

extern "C" {
#include <hicn/ctrl/hicn-light.h>
}

namespace transport {

namespace core {

class UdpTunnelConnector;

class HicnForwarderModule : public IoModule {
  static constexpr std::uint16_t interface_mtu = 1500;

 public:
#if 0
  union addressLight {
    uint32_t ipv4;
    struct in6_addr ipv6;
  };

  struct route_to_self_command {
    uint8_t messageType;
    uint8_t commandID;
    uint16_t length;
    uint32_t seqNum;
    char symbolicOrConnid[16];
    union addressLight address;
    uint16_t cost;
    uint8_t addressType;
    uint8_t len;
  };

  using route_to_self_command = struct route_to_self_command;
#endif

  HicnForwarderModule();

  ~HicnForwarderModule();

  void connect(bool is_consumer) override;

  void send(Packet &packet) override;
  void send(const utils::MemBuf::Ptr &buffer) override;

  bool isConnected() override;

  void init(Connector::PacketReceivedCallback &&receive_callback,
            Connector::PacketSentCallback &&sent_callback,
            Connector::OnCloseCallback &&close_callback,
            Connector::OnReconnectCallback &&reconnect_callback,
            asio::io_service &io_service,
            const std::string &app_name = "Libtransport") override;

  void registerRoute(const Prefix &prefix) override;

  void sendMapme() override;

  void setForwardingStrategy(const Prefix &prefix,
                             std::string &strategy) override;

  std::uint32_t getMtu() override;

  bool isControlMessage(utils::MemBuf &packet_buffer) override;

  void processControlMessageReply(utils::MemBuf &packet_buffer) override;

  void closeConnection() override;

 private:
  utils::MemBuf::Ptr createCommandRoute(std::unique_ptr<sockaddr> &&addr,
                                        uint8_t prefix_length);
  utils::MemBuf::Ptr createCommandDeleteConnection();
  utils::MemBuf::Ptr createCommandMapmeSendUpdate();
  utils::MemBuf::Ptr createCommandSetForwardingStrategy(
      std::unique_ptr<sockaddr> &&addr, uint32_t prefix_len,
      std::string strategy);

  static void parseForwarderConfiguration(const libconfig::Setting &io_config,
                                          std::error_code &ec);
  static std::string initForwarderUrl();

 private:
  std::shared_ptr<UdpTunnelConnector> connector_;
  /* Sequence number used for sending control messages */
  uint32_t seq_;

  class ForwarderUrlInitializer {
    static inline char default_hicnlight_url[] = "hicn://127.0.0.1:9695";
    static inline char hicnlight_configuration_section[] = "hicnlight";

   public:
    ForwarderUrlInitializer()
        : forwarder_url_(ForwarderUrlInitializer::default_hicnlight_url) {
      using namespace std::placeholders;
      GlobalConfiguration::getInstance().registerConfigurationParser(
          ForwarderUrlInitializer::hicnlight_configuration_section,
          std::bind(&ForwarderUrlInitializer::parseForwarderConfiguration, this,
                    _1, _2));
    }

    std::string getForwarderUrl() { return forwarder_url_; }

   private:
    void parseForwarderConfiguration(const libconfig::Setting &forwarder_config,
                                     std::error_code &ec) {
      using namespace libconfig;

      // forwarder url hicn://127.0.0.1:12345
      if (forwarder_config.exists("forwarder_url")) {
        // Get number of threads
        forwarder_config.lookupValue("forwarder_url", forwarder_url_);
        VLOG(1) << "Forwarder URL from config file: " << forwarder_url_;
      }
    }

    // Url of the forwarder
    std::string forwarder_url_;
  };

  static ForwarderUrlInitializer forwarder_url_initializer_;
};

extern "C" IoModule *create_module(void);

}  // namespace core

}  // namespace transport
