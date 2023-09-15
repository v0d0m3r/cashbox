#include <iostream>
#include <algorithm>
#include <vector>
#include <memory>
#include <cstring>
#include <numeric>
#include <cmath>
#include <array>
#include <fstream>
#include <ostream>
#include <ios>
#include <mutex>
#include <sstream>
#include <ctime>
#include <syncstream>

//------------------------------------------------------------------------------

inline std::string now_date_time()
{
  time_t now = time(0);
  struct tm tstruct;
  char buf[24];
#ifdef _WIN32
  localtime_s(&now, &tstruct);
#elif __gnu_linux__
  localtime_r(&now, &tstruct);
#else
  tstruct = *localtime(&now);
#endif
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
  return buf;
}

//------------------------------------------------------------------------------

inline std::string now_date()
{
  time_t now = time(0);
  struct tm tstruct;
  char buf[24];
#ifdef _WIN32
  localtime_s(&tstruct, &now);
#elif __gnu_linux__
  localtime_r(&now, &tstruct);
#else
  tstruct = *localtime(&now);
#endif
  strftime(buf, sizeof(buf), "%Y%m%d", &tstruct);
  return buf;
}

//------------------------------------------------------------------------------

enum class Lg_lvl {
  error, warning, info, debug,
  debug1, debug2, debug3, debug4
};

//------------------------------------------------------------------------------

inline int lg_lvl_to_int(Lg_lvl ll)
{
  return static_cast<int>(ll);
}

//------------------------------------------------------------------------------

template<class Digits>
Lg_lvl to_lg_lvl(const Digits& ll)
{
  if (       ll > Digits(Lg_lvl::debug4)
      || ll < Digits(Lg_lvl::error))
    throw std::runtime_error("int_to_lg_lvl(): bad Log level");
  return static_cast<Lg_lvl>(ll);
}

//------------------------------------------------------------------------------

inline const std::vector<std::string>& lg_lvl_names()
{
  static const std::vector<std::string> vs {
    "ERROR:   ",
    "WARNING: ",
    "INFO:    ",
    "DEBUG:   ",
    "DEBUG1:  ",
    "DEBUG2:  ",
    "DEBUG3:  ",
    "DEBUG4:  "
  };
  return vs;
}

//------------------------------------------------------------------------------

inline const std::string& lg_lvl_to_string(Lg_lvl ll)
{
  static const std::vector<std::string>& names{lg_lvl_names()};
  return names[static_cast<size_t>(lg_lvl_to_int(ll))];
}

//------------------------------------------------------------------------------

inline bool in_lg_lvl_name(const std::string& s1, const std::string& s2)
{
  return search(s1.begin(), s1.end(), s2.begin(), s2.end()) != s1.end();
}

//------------------------------------------------------------------------------

inline Lg_lvl string_to_lg_lvl(const std::string& ll)
{
  const std::vector<std::string>& names{lg_lvl_names()};

  for (size_t i{0}; i < names.size(); ++i)
    if (in_lg_lvl_name(names[i], ll))
      return to_lg_lvl(i);
  return Lg_lvl::info;
}

//------------------------------------------------------------------------------

// Common mechanism of logging, that uses iostream;
// Uses logging levels;
class Logger_wrap {
protected:
  std::ostream& out;
  Lg_lvl report;
  Lg_lvl curr;
  std::ostringstream buf;

public:
  class Tmp_log;

  explicit Logger_wrap(std::ostream& os, Lg_lvl ll = Lg_lvl::info)
    : out(os), report(ll), curr(ll)
  {
    if (!out)
      throw std::runtime_error("Logger_wrap: bad out");
  }
  virtual ~Logger_wrap() {}

  Logger_wrap& operator() (Lg_lvl ll = Lg_lvl::info)
  { curr = ll; return *this; }

  virtual void write(const std::string& str)
  {
    if (!out || str.empty())
      return;
    out << now_date_time() << ' ' << lg_lvl_to_string(curr)
        << str << '\n';
    out.flush();
  }

  template<class T>
  Tmp_log operator<<(const T& data);

  Logger_wrap(const Logger_wrap&) = delete;
  Logger_wrap& operator=(const Logger_wrap&) = delete;
};

//------------------------------------------------------------------------------

class Logger_wrap::Tmp_log {
  friend class Logger_wrap;
public:
  ~Tmp_log() noexcept(false)  // to suppress warning C4297 (win32)
                              // it warns that the destructor
                              // is throwing exception, although
                              // there is catchall block
  try {
    if (buf) {
      logger.write(buf->str());
      buf->str("");
    }
  }
  catch (...) {

  }

  template<class T>
  Tmp_log& operator<<(const T& data)
  {
    if (buf)
      *buf << data;
    return *this;
  }

  Tmp_log(const Tmp_log&) = delete;

private:
  Logger_wrap& logger;
  std::ostringstream* buf;

  Tmp_log(Logger_wrap& logger_, std::ostringstream* buf_)
    : logger(logger_), buf(buf_) { }

  Tmp_log(Tmp_log&& that)
    : logger(that.logger), buf(that.buf)
  { that.buf = nullptr; }
};

//------------------------------------------------------------------------------

template<class T>
Logger_wrap::Tmp_log Logger_wrap::operator<<(const T& data)
{
  Tmp_log tmp(*this, curr > report ? nullptr : &buf);
  return std::move(tmp << data);
}

//------------------------------------------------------------------------------

// Represents mechanism of logging, that uses iostream;
// Main feature: opens file only when there is the first write;
class Flogger : public Logger_wrap {
  std::ofstream& ofs;
  std::string fnm;
public:
  Flogger(std::ofstream& of, const std::string& fname, Lg_lvl ll = Lg_lvl::info)
    : Logger_wrap{of, ll}, ofs(of), fnm{fname}  {}

  virtual void write(const std::string& str) override
  {
    if (!ofs.is_open()) {
      ofs.open(fnm, std::ios_base::app);
      if (!ofs.is_open())
        return;
    }
    Logger_wrap::write(str);
  }
  ~Flogger() { if (ofs.is_open()) ofs.close(); }
};

//------------------------------------------------------------------------------

// Like Logger_wrap, but with addition of synchronization to ostream;
class Logger_wrap_sync : public Logger_wrap {
public:
  using Logger_wrap::Logger_wrap;

  virtual void write(const std::string& str) override
  {
    std::osyncstream sout{out};
    if (!sout || str.empty())
      return;
    sout << now_date_time() + ' ' + lg_lvl_to_string(curr) + str + '\n';
    sout.flush();
  }
};

//------------------------------------------------------------------------------

// Allow threadsafe operations open and close on ofstream;
// Invariant: ofs_ is NOT accessed anywhere without any synchronization
// (due to reference(), invoking code should be in charge of the invariant);
class Sync_ofstream_open_close {
  std::mutex mutex_;
  std::unique_ptr<std::ofstream> ofs_;
  std::string fnm_;
  bool is_open_;
public:

  explicit Sync_ofstream_open_close(std::string fname)
    : ofs_{std::make_unique<std::ofstream>()},
      fnm_{std::move(fname)},
      is_open_{false}
  {}

  Sync_ofstream_open_close(const Sync_ofstream_open_close&) = delete;
  Sync_ofstream_open_close& operator=(const Sync_ofstream_open_close&) = delete;

  Sync_ofstream_open_close(Sync_ofstream_open_close&&) = delete;
  Sync_ofstream_open_close& operator=(Sync_ofstream_open_close&&) = delete;

  bool is_open() const noexcept { return is_open_; }

  bool try_open()
  {
    std::lock_guard guard{mutex_};
    if (!ofs_->is_open()) {
      ofs_->open(fnm_, std::ios_base::app);
      is_open_ = ofs_->is_open();
    }
    else
      is_open_ = true;
    return is_open_;
  }

  // be caution, exposes reference to underlying ofstream
  std::ostream& reference() const
  {
    if (!ofs_)
      throw std::runtime_error("Sync_ofstream_open_close::reference(): ofs is nullptr");
    return *(ofs_.get());
  }

  ~Sync_ofstream_open_close() noexcept(false) // to suppress warning C4297 (win32)
                                              // it warns that the destructor
                                              // is throwing exception, although
                                              // there is catchall block
  try {
    std::lock_guard guard{mutex_};
    if (ofs_->is_open())
      ofs_->close();
  }
  catch(...) {
  }
};

//------------------------------------------------------------------------------

// Represent synchronized version of Flogger;
class Flogger_sync : public Logger_wrap_sync {
  std::shared_ptr<Sync_ofstream_open_close> data_;
  void (Flogger_sync::*current_func)(const std::string& str);

  void open_file(const std::string& str)
  {
    if (!data_->is_open() && !data_->try_open())
      throw std::runtime_error("Cannot open file");

    write_after_open(str);
    current_func = &Flogger_sync::write_after_open;
  }

  void write_after_open(const std::string& str) { Logger_wrap_sync::write(str); }

public:
  explicit Flogger_sync(const std::shared_ptr<Sync_ofstream_open_close>& data,
    Lg_lvl ll = Lg_lvl::info)
    : Logger_wrap_sync{data->reference(), ll}, data_{data},
      current_func{&Flogger_sync::open_file}
  {
    if (!out)
      throw std::runtime_error("Flogger_sync(): bad state of out");
  }

  virtual void write(const std::string& str) override
  {
    (this->*current_func) (str);
  }
};

//------------------------------------------------------------------------------
