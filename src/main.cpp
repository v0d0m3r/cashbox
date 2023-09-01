#include "library/core/Logger_wrap.hpp"
#include <thread>

//------------------------------------------------------------------------------

void test_loggers()
{
  auto sync_data_stream{std::make_shared<Sync_ofstream_open_close>("tmp.txt")};

  auto worker {
    [](std::string id, const std::size_t start, const std::size_t stop,
       std::shared_ptr<Sync_ofstream_open_close> data) {
    auto slout{Flogger_sync{data}};
    auto sout{Logger_wrap_sync{std::cout}};
    for (auto idx{start}; idx < stop; ++idx ) {
      sout << "thread: " << id  << "; work: " << idx;
      slout << "thread: " << id  << "; work: " << idx;
    }
  }};

  std::jthread t1{worker, "1", 10000, 20000, sync_data_stream};
  std::jthread t2{worker, "2", 20000, 30000, sync_data_stream};
  std::jthread t3{worker, "3", 40000, 50000, sync_data_stream};
  std::jthread t4{worker, "4", 60000, 70000, sync_data_stream};
}

//------------------------------------------------------------------------------

int main()
{
  test_loggers();
  return 0;
}


//------------------------------------------------------------------------------
