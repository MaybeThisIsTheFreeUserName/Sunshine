/**
 * @file tests/unit/platform/windows/test_misc.cpp
 * @brief Tests for Windows thread priorities and UDP batching.
 */

// test includes
#include "../../../tests_common.h"

#ifdef _WIN32
  // standard includes
  #include <array>
  #include <utility>
  #include <vector>

  // lib includes
  #include <boost/asio.hpp>

  // platform includes
  #include <Windows.h>

  // local includes
  #include <src/platform/common.h>
  #include <src/utility.h>

TEST(WindowsThreadPriorityTest, MapsEveryPriorityAndIgnoresUnknownValues) {
  using enum platf::thread_priority_e;
  const auto thread = GetCurrentThread();
  const auto original_priority = GetThreadPriority(thread);
  ASSERT_NE(original_priority, THREAD_PRIORITY_ERROR_RETURN);
  const auto restore_priority = util::fail_guard([thread, original_priority]() {
    EXPECT_TRUE(SetThreadPriority(thread, original_priority));
  });
  const std::array cases {
    std::pair {low, THREAD_PRIORITY_BELOW_NORMAL},
    std::pair {normal, THREAD_PRIORITY_NORMAL},
    std::pair {high, THREAD_PRIORITY_ABOVE_NORMAL},
    std::pair {critical, THREAD_PRIORITY_HIGHEST},
  };
  for (const auto &[priority, expected] : cases) {
    platf::adjust_thread_priority(priority);
    EXPECT_EQ(GetThreadPriority(thread), expected);
  }
  platf::adjust_thread_priority(static_cast<platf::thread_priority_e>(-1));
  EXPECT_EQ(GetThreadPriority(thread), THREAD_PRIORITY_HIGHEST);
}

/**
 * @brief Valid UDP batches must select individual sends, even when the OS supports USO.
 */
TEST(WindowsUdpBatchTest, RejectsSingleAndMultipleDatagramsWithAndWithoutHeaders) {
  boost::asio::io_context io_context;
  auto address = boost::asio::ip::address_v4::loopback();
  boost::asio::ip::address source_address = address;
  boost::asio::ip::address target_address = address;
  boost::asio::ip::udp::socket receiver(io_context, {address, 0});
  boost::asio::ip::udp::socket sender(io_context, {address, 0});
  const std::array<char, 12> payload {};
  const std::array<char, 6> headers {};
  std::vector<platf::buffer_descriptor_t> buffers {{payload.data(), payload.size()}};
  auto batch = platf::batched_send_info_t {
    nullptr,
    0,
    buffers,
    4,
    1,
    1,
    static_cast<std::uintptr_t>(sender.native_handle()),
    target_address,
    receiver.local_endpoint().port(),
    source_address,
  };

  for (const auto *header : {static_cast<const char *>(nullptr), headers.data()}) {
    batch.headers = header;
    batch.header_size = header ? 2 : 0;
    for (const auto count : {1, 2}) {
      batch.block_count = count;
      EXPECT_FALSE(platf::send_batch(batch));
    }
  }
}
#endif
