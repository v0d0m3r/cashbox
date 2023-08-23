#include "library/Logger_wrap.hpp"

//------------------------------------------------------------------------------

int main()
{
  auto sync_data_stream{std::make_shared<Sync_ofstream_open_close>("tmp.txt")};
  auto slout{Flogger_sync{sync_data_stream}};
  auto slout2{Flogger_sync{sync_data_stream}};
  slout << "test1" << " good morning " << 1;
  slout2 << "test2" << " good evening " << 2;
  return 0;
}

//------------------------------------------------------------------------------
