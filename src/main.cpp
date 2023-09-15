#include "library/core/Logger_wrap.hpp"
#include <thread>

//------------------------------------------------------------------------------

struct Simple_range {
  const size_t start;
  const size_t stop;
};

//------------------------------------------------------------------------------

void test_loggers()
{
  auto sync_data_stream{std::make_shared<Sync_ofstream_open_close>("tmp.txt")};

  auto worker {
    [](const std::string& ids, const Simple_range& simple_range,
       const std::shared_ptr<Sync_ofstream_open_close>& data) {
    auto slout{Flogger_sync{data}};
    auto sout{Logger_wrap_sync{std::cout}};
    for (auto idx{simple_range.start}; idx < simple_range.stop; ++idx ) {
      sout << "thread: " << ids  << "; work: " << idx;
      slout << "thread: " << ids  << "; work: " << idx;
    }
  }};

  const std::jthread thread1{
    worker, "1", Simple_range{10000, 20000}, sync_data_stream};
  const std::jthread thread2{
    worker, "2", Simple_range{20000, 30000}, sync_data_stream};
  const std::jthread thread3{
    worker, "3", Simple_range{40000, 50000}, sync_data_stream};
  const std::jthread thread4{
    worker, "4", Simple_range{60000, 70000}, sync_data_stream};
}

//------------------------------------------------------------------------------

int main() noexcept(false)  // to suppress warning [bugprone-exception-escape]
                            // it warns that the destructor
                            // is throwing exception, although
                            // there is catchall block
try {
  test_loggers();
  return 0;
}
catch (...) {
  std::cerr << "Error occured exiting" << std::endl;
}

//------------------------------------------------------------------------------
