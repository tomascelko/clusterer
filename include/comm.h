#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/icmp.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>
#pragma once
// reader capable of offsetting hits and modifying frequency of data stream
using namespace std::chrono_literals;
enum class command_type {
  SET_ACQ_TIME_LSB = 0x1,
  SET_BIAS_VOLTAGE = 0x2,
  START_ACQ = 0x3,
  STOP_ACQ = 0x6,
  SET_ACQ_MODE = 0x9,
  SET_ACQ_TIME_MSB = 0xa,
  GET_CHIP_ID = 0xb,
  GET_BIAS_VOLTAGE = 0xc,
  GET_READOUT_STATE = 0x17,
  GET_SENSOR_TEMP = 0x19
};

enum class measurement_data_type {
  KATHERINE_1_ID = 0x0,
  KATHERINE_2_ID = 0x1,
  PIXEL_MEASUREMENT_DATA = 0x4,
  PIXEL_TIMESTAMP_OFFSET = 0x5,
  CURRENT_FRAME_FINISHED = 0xC,
  LOST_PIXEL_COUNT = 0xD

};

class katherine_parser {
  const uint16_t FRAME_TYPE_OFFSET = 44;
  const uint16_t COORDINATE_OFFSET = 28;
  const uint16_t TOA_OFFSET = 14;
  const uint16_t TOT_OFFSET = 4;
  uint32_t time_offset_ = 0;

public:
  uint64_t total_hits_received = 0;
  uint32_t time_offset() { return time_offset_; }
  std::string parse_chip_id(char *response) {
    std::cout << "Chip iD ";
    int x = (int)(response[0] & 0xf) - 1;
    int y = (int)(response[0] >> 4 & 0xf);
    int w = (int)(response[2] << 4 | response[1]);
    std::stringstream chip_id{};
    chip_id << std::string{char('A' + (char)x)} << y << "-W000" << w
            << std::endl;
    return chip_id.str();
  }
  double parse_single_float(char *response) {
    float result;
    std::memcpy(&result, response, sizeof(float));
    return result;
  }
  burda_hit parse_burda_hit(int64_t data, uint32_t offset) {

    data = data & 0xfffffffffffL;
    uint16_t linear_coord = (data & 0xfffffffffffL) >> 28;
    // data = data & 0xfffffffL;
    int64_t toa = ((data & 0xfffffffL) >> 14) + 16384 * offset;
    // data = data & 0x3fffL;
    int16_t tot = (data & 0x3fffL) >> 4;
    uint8_t fast_toa = data & 0xfL;
    return burda_hit{linear_coord, toa, fast_toa, tot};
    // std::cout << "Hit data " << std::setprecision(10) << hit.linear_coord()
    // << " " << hit.toa() << " " << hit.fast_toa() << " " << hit.tot() <<
    // std::endl; return hit;
  }

  bool parse_6byte_data_frame(int64_t data_frame,
                              std::vector<burda_hit> &hits) {

    switch (static_cast<measurement_data_type>(data_frame >> 44)) {
    case measurement_data_type::PIXEL_MEASUREMENT_DATA:
    case measurement_data_type::KATHERINE_1_ID:
    case measurement_data_type::KATHERINE_2_ID:
      hits.emplace_back(parse_burda_hit(data_frame, time_offset_));
      break;
    case measurement_data_type::PIXEL_TIMESTAMP_OFFSET:
      time_offset_ = data_frame & 0xffffffff;
      // std::cout << "Time offset updated to " << time_offset_ << std::endl;
      break;
    case measurement_data_type::CURRENT_FRAME_FINISHED:
      total_hits_received = (data_frame & 0x0fffffffff);
      std::cout << "TOTAL HITS RECEIVED: " << total_hits_received << std::endl;
      return true;
      break;
    case measurement_data_type::LOST_PIXEL_COUNT:
      std::cout << "Lost pixels in Katherine: " << (data_frame & 0x0fffffffff)
                << std::endl;
      return true;
    default:
      // std::cout << std::hex << ( data_frame >> 44) << std::endl;
      // std:: cout << "FRAME " << (data_frame >> 44) << std::endl;
      break;
    }
    return false;
  }
  std::vector<burda_hit> parse_udp_data_packet(char *packet, size_t packet_size,
                                               bool &is_final_df) {
    // std::cout << "Packet size" << packet_size << std::endl;
    const uint32_t DF_SIZE = 6;
    std::vector<burda_hit> hits;
    hits.reserve(65536 / 6);
    // std::cout << packet_size << std::endl;
    for (size_t df_index = 0; df_index < packet_size; df_index += DF_SIZE) {
      int64_t df_int = 0x0;
      std::memcpy(&df_int, packet + df_index * sizeof(char),
                  DF_SIZE * sizeof(char));
      // std::cout << df_index << std::endl;
      is_final_df = parse_6byte_data_frame(df_int, hits);
    }
    return hits;
  }
};

class udp_controller {
  // set RMEM MAX > 1 000 000 000, corresponding to
  const uint16_t CONTROL_PORT = 1555;
  const uint16_t DATA_PORT = 1556;
  const uint32_t CMD_BYTE_INDEX = 6;
  const uint32_t PACKET_LEN = 8;
  const uint64_t ACQ_TIME = 10;
  const uint64_t UDP_OS_BUFFER_SIZE = 2 << 28; // 2 << 28; //

  const uint64_t UDP_BOOST_BUFFER_SIZE = 2 << 18; // 18
  const float BIAS_VOLTAGE = 200.;

public:
  boost::asio::io_service service;
  boost::asio::ip::udp::socket local_control_socket;
  boost::asio::ip::udp::endpoint remote_control_endpoint;

  boost::asio::ip::udp::socket local_data_socket;
  boost::asio::ip::udp::endpoint remote_data_endpoint;

  boost::asio::ip::udp::endpoint local_endpoint{boost::asio::ip::udp::v4(),
                                                CONTROL_PORT};
  boost::asio::ip::udp::endpoint local_data_endpoint{boost::asio::ip::udp::v4(),
                                                     DATA_PORT};

  boost::asio::ip::udp::endpoint sender_endpoint;

  std::function<void(std::vector<burda_hit> &&)> process_packet_callback;
  std::function<void()> acq_end_callback;

  katherine_parser parser{};

  std::vector<char *> responses = {
      new char[8], new char[8], new char[8], new char[8], new char[8],
      new char[8], new char[8], new char[8], new char[8], new char[8]};
  char *data_response = new char[UDP_BOOST_BUFFER_SIZE]{0x0};
  uint32_t command_index = 0;

  uint32_t time_offset = 0;

  int32_t no_response_count = 0;

  bool wainting_for_response = false;
  void setup_control() {
    local_control_socket.open(local_endpoint.protocol());
    local_control_socket.bind(local_endpoint);

    local_data_socket.open(local_data_endpoint.protocol());
    local_data_socket.bind(local_data_endpoint);
  }
  void send_command(char *command, bool wait_for_reply = true) {

    local_control_socket.send_to(boost::asio::buffer(command, 8),
                                 remote_control_endpoint);
    if (wait_for_reply)
      local_control_socket.receive_from(boost::asio::buffer(responses[0], 8),
                                        remote_control_endpoint);
  }
  void async_send_command(char *command) {
    // std::cout << "sending... ";
    // std::this_thread::sleep_for(500ms);
    // for (int i = 0; i < 8; ++i) {
    // std::cout << std::setw(2) << std::setfill('0') << std::hex <<
    // (int)(command[i]) << "|";
    //}
    // std::cout << std::endl;
    local_control_socket.async_send_to(
        boost::asio::buffer(command, 8), remote_control_endpoint,
        std::bind(&udp_controller::on_control_send, this, command_index,
                  std::placeholders::_1, std::placeholders::_2));
    ++command_index;
  }
  void on_control_send(uint32_t command_index,
                       const boost::system::error_code &error,
                       std::size_t bytes_transferred) {
    // std::cout << "Command sent" << std::endl;
    if (!error) {
      std::cout << "Packet sent successfully." << std::endl;
      local_control_socket.async_receive_from(
          boost::asio::buffer(responses[command_index], 8), sender_endpoint,
          std::bind(&udp_controller::on_control_receive, this, command_index,
                    std::placeholders::_1, std::placeholders::_2,
                    sender_endpoint));
    } else {
      std::cerr << "Send error: " << error.message() << std::endl;
    }
  }

  void on_control_receive(uint32_t command_index,
                          const boost::system::error_code &error,
                          std::size_t bytes_transferred,
                          boost::asio::ip::udp::endpoint sender_endpoint) {
    if (!error) {

      std::cout << "Packet succesfully received" << std::endl;
      std::cout << "Response:";
      std::cout << "Command index" << command_index << std::endl;
      // for (uint32_t byte_offset = 0; byte_offset < bytes_transferred;
      // ++byte_offset)
      //     std::cout << std::setw(2) << std::setfill('0') << std::hex <<
      //     static_cast<int>(responses[command_index][byte_offset]) << "|";
      if (responses[command_index][6] ==
          static_cast<char>(command_type::GET_CHIP_ID)) {
        std::cout << "Chip ID" << parser.parse_chip_id(responses[command_index])
                  << std::endl;
      }
      if (responses[command_index][6] ==
          static_cast<char>(command_type::GET_SENSOR_TEMP)) {
        std::cout << "Sensor temp "
                  << parser.parse_single_float(responses[command_index])
                  << std::endl;
      }
      if (responses[command_index][6] ==
          static_cast<char>(command_type::GET_BIAS_VOLTAGE)) {
        std::cout << "Bias voltage "
                  << parser.parse_single_float(responses[command_index])
                  << std::endl;
      }
      std::cout << std::endl;

    } else {
      std::cout << "Encountered error " << error.message() << std::endl;
    }
  }
  boost::asio::ip::udp::endpoint sender_endpoint1;
  std::function<void(const boost::system::error_code &, std::size_t)>
      on_data_receive_handler = [this](const boost::system::error_code &error,
                                       std::size_t bytes_transferred) {
        on_data_receive(error, bytes_transferred, sender_endpoint1);
      };
  int32_t waiting_handlers_count = 0;
  uint32_t waiting_handlers_expected_count = 1000;

  void on_data_receive(const boost::system::error_code &error,
                       std::size_t bytes_transferred,
                       boost::asio::ip::udp::endpoint sender_endpoint) {
    bool acq_end = false;
    // for (uint32_t byte_offset = 0; byte_offset < bytes_transferred;
    // ++byte_offset)
    //         std::cout << std::setw(2) << std::setfill('0') << std::hex <<
    //         static_cast<uint>(data_response[byte_offset]) << "/";
    process_packet_callback(parser.parse_udp_data_packet(
        data_response, bytes_transferred, acq_end));
    boost::asio::ip::udp::endpoint sender_endpoint1;
    --waiting_handlers_count;
    if (!acq_end) {
      // if(waiting_handlers_count < waiting_handlers_expected_count - 100)
      // for(int i = 0; i < waiting_handlers_expected_count -
      // waiting_handlers_count; i++)
      //{
      local_data_socket.async_receive_from(
          boost::asio::buffer(data_response, UDP_BOOST_BUFFER_SIZE),
          sender_endpoint1, on_data_receive_handler);
      //}
      // std::bind(&udp_controller::on_data_receive, this,
      // std::placeholders::_1, std::placeholders::_2, sender_endpoint1));
    } else {
      std::cout << "Acquisition ended" << std::endl;
      service.stop();
      acq_end_callback();
    }
  }

  std::vector<burda_hit> sync_read_data() {
    bool acq_end = false;
    boost::asio::ip::udp::endpoint sender_endpoint1;
    auto bytes_transferred = local_data_socket.receive_from(
        boost::asio::buffer(data_response, UDP_BOOST_BUFFER_SIZE),
        sender_endpoint1);
    auto hits =
        parser.parse_udp_data_packet(data_response, bytes_transferred, acq_end);
    // for (uint32_t byte_offset = 0; byte_offset < bytes_transferred;
    // ++byte_offset)
    //         std::cout << std::setw(2) << std::setfill('0') << std::hex <<
    //         static_cast<uint>(data_response[byte_offset]) << "/";
    if (acq_end) {
      acq_end_callback();
      std::cout << "Acquisition ended" << std::endl;
    }
    return hits;
  }
  char *set_start_acq_mode(char *mode) {

    char *command = new char[PACKET_LEN];
    command[0] = mode[0];
    command[1] = mode[1];
    return command;
  }

  udp_controller(
      const std::string &ip,
      std::function<void(std::vector<burda_hit> &&)> &&packet_callback,
      std::function<void()> &&on_end_callback)
      : local_control_socket(service),
        remote_control_endpoint(boost::asio::ip::address::from_string(ip),
                                CONTROL_PORT),
        local_data_socket(service),
        remote_data_endpoint(boost::asio::ip::address::from_string(ip),
                             DATA_PORT),
        process_packet_callback(packet_callback),
        acq_end_callback(on_end_callback) {
    setup_control();
    local_data_socket.set_option(
        boost::asio::socket_base::receive_buffer_size(UDP_OS_BUFFER_SIZE));
  }
  udp_controller(const std::string &ip, std::function<void()> &&on_end_callback)
      : local_control_socket(service),
        remote_control_endpoint(boost::asio::ip::address::from_string(ip),
                                CONTROL_PORT),
        local_data_socket(service),
        remote_data_endpoint(boost::asio::ip::address::from_string(ip),
                             DATA_PORT),
        acq_end_callback(on_end_callback) {
    setup_control();
    local_data_socket.set_option(
        boost::asio::socket_base::receive_buffer_size(UDP_OS_BUFFER_SIZE));
  }

  void async_start_acq() {
    char *command = new char[8]{};

    command[CMD_BYTE_INDEX] = static_cast<char>(command_type::GET_SENSOR_TEMP);
    async_send_command(command);

    char *command_BIAS = new char[8]{};
    float bias = BIAS_VOLTAGE;
    std::memcpy(command_BIAS, &bias, 4);
    command_BIAS[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::SET_BIAS_VOLTAGE);

    async_send_command(command_BIAS);

    char *command3 = new char[8]{};

    command3[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::GET_BIAS_VOLTAGE);
    async_send_command(command3);
    async_send_command(command3);

    char *command2 = new char[8]{};
    // command2[CMD_BYTE_INDEX] = static_cast<char>(command_type::GET_CHIP_ID);
    // async_send_command(command2);

    char *command4 = new char[8]{};

    std::copy(static_cast<const char *>(static_cast<const void *>(&ACQ_TIME)),
              static_cast<const char *>(static_cast<const void *>(&ACQ_TIME)) +
                  4,
              command4);
    command4[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::SET_ACQ_TIME_LSB);
    uint64_t acq_time = ACQ_TIME * 100000000;
    for (int i = 0; i < 4; ++i)
      command4[i] = static_cast<char>(acq_time >> (i * 8)) & 0xFF;
    async_send_command(command4);

    char *command5 = new char[8]{};

    command5[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::SET_ACQ_TIME_MSB);
    // std::copy(static_cast<const char *>(static_cast<const void *>(&ACQ_TIME))
    // + 4,
    //     static_cast<const char *>(static_cast<const void *>(&ACQ_TIME)) + 8,
    //     command5);

    for (int i = 4; i < 8; ++i)
      command5[i - 4] = static_cast<char>(acq_time >> (i * 8)) & 0xFF;
    async_send_command(command5);

    char *command6 = set_start_acq_mode(new char[2]{0x1, 0x1});
    command6[CMD_BYTE_INDEX] = static_cast<char>(command_type::START_ACQ);
    async_send_command(command6);

    boost::asio::ip::udp::endpoint sender_endpoint1;
    local_data_socket.async_receive_from(
        boost::asio::buffer(data_response, UDP_BOOST_BUFFER_SIZE),
        sender_endpoint,
        std::bind(&udp_controller::on_data_receive, this, std::placeholders::_1,
                  std::placeholders::_2, sender_endpoint));

    service.run();
  }
  void sync_start_acq() {
    char *command = new char[8]{};

    command[CMD_BYTE_INDEX] = static_cast<char>(command_type::GET_SENSOR_TEMP);
    send_command(command);

    char *command_BIAS = new char[8]{};
    float bias = BIAS_VOLTAGE;
    std::memcpy(command_BIAS, &bias, 4);
    command_BIAS[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::SET_BIAS_VOLTAGE);

    send_command(command_BIAS);

    char *command3 = new char[8]{};

    command3[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::GET_BIAS_VOLTAGE);
    send_command(command3);
    send_command(command3);

    char *command2 = new char[8]{};
    // command2[CMD_BYTE_INDEX] = static_cast<char>(command_type::GET_CHIP_ID);
    // async_send_command(command2);

    char *command4 = new char[8]{};

    std::copy(static_cast<const char *>(static_cast<const void *>(&ACQ_TIME)),
              static_cast<const char *>(static_cast<const void *>(&ACQ_TIME)) +
                  4,
              command4);
    command4[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::SET_ACQ_TIME_LSB);
    uint64_t acq_time = ACQ_TIME * 100000000;
    for (int i = 0; i < 4; ++i)
      command4[i] = static_cast<char>(acq_time >> (i * 8)) & 0xFF;
    send_command(command4);

    char *command5 = new char[8]{};

    command5[CMD_BYTE_INDEX] =
        static_cast<char>(command_type::SET_ACQ_TIME_MSB);
    // std::copy(static_cast<const char *>(static_cast<const void *>(&ACQ_TIME))
    // + 4,
    //     static_cast<const char *>(static_cast<const void *>(&ACQ_TIME)) + 8,
    //     command5);

    for (int i = 4; i < 8; ++i)
      command5[i - 4] = static_cast<char>(acq_time >> (i * 8)) & 0xFF;
    send_command(command5);

    char *command6 = set_start_acq_mode(new char[2]{0x1, 0x1});
    command6[CMD_BYTE_INDEX] = static_cast<char>(command_type::START_ACQ);
    send_command(command6, false);
  }
};