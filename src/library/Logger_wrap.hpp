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
  using namespace std::chrono;
  time_t now = time(0);
  struct tm tstruct;
  char buf[24];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
  return buf;
}

//------------------------------------------------------------------------------

inline std::string now_date()
{
  time_t now = time(0);
  struct tm tstruct;
  char buf[24];
  tstruct = *localtime(&now);
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

template<typename Digits>
Lg_lvl to_lg_lvl(const Digits& ll)
{
  if (       ll > Digits(Lg_lvl::debug4)
      || ll < Digits(Lg_lvl::error))
    std::runtime_error("int_to_lg_lvl(): bad Log level");
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
    if (in_lg_lvl_name(names[i], ll)) return to_lg_lvl(i);
  return Lg_lvl::info;
}

//------------------------------------------------------------------------------

// Общий механизм логирования;
// В качестве интерфейса вывода используется
// любой поток-наследник ostream;
// В качестве разделения логируемой информации
// используются уровни логирования;
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
      std::runtime_error("Logger_wrap: bad out");
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

  template<typename T>
  Tmp_log operator<<(const T& data);

  Logger_wrap(const Logger_wrap&) = delete;
  Logger_wrap& operator=(const Logger_wrap&) = delete;
};

//------------------------------------------------------------------------------

class Logger_wrap::Tmp_log
{
  friend class Logger_wrap;
public:
  ~Tmp_log()
  try {
    if (buf) {
      logger.write(buf->str());
      buf->str("");
    }
  }
  catch (const std::exception&) {

  }

  template<typename T>
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

  Tmp_log(Logger_wrap& logger_,
    std::ostringstream* buf_) :
                                logger(logger_), buf(buf_) { }

  Tmp_log(Tmp_log&& that):
                            logger(that.logger), buf(that.buf)
  { that.buf = nullptr; }
};

//------------------------------------------------------------------------------

template<typename T>
Logger_wrap::Tmp_log Logger_wrap::operator<<(const T& data)
{
  Tmp_log tmp(*this, curr > report ? nullptr : &buf);
  return std::move(tmp << data);
}

//------------------------------------------------------------------------------

// Представляет собой механизм логирования,
// основанный на файловом вводе-выводе;
// Главная особенность: не создает/открывает файл,
// пока не будет выведено первое сообщение
class Flogger : public Logger_wrap {
  std::ofstream& ofs;
  std::string fnm;
public:
  Flogger(std::ofstream& of, const std::string& fname, Lg_lvl ll = Lg_lvl::info)
    : Logger_wrap{of, ll}, ofs(of), fnm{fname}  {}

  virtual void write(const std::string& str) override
  {
    if (!ofs.is_open()) {               // Открываем файл, только
      // в момент первой записи
      ofs.open(fnm, std::ios_base::app);
      if (!ofs.is_open()) return;
    }
    Logger_wrap::write(str);
  }
  ~Flogger() { if (ofs.is_open()) ofs.close(); }
};

//------------------------------------------------------------------------------

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
    if (!ofs_->is_open()) {     // Открываем файл, только
      // в момент первой записи
      ofs_->open(fnm_, std::ios_base::app);
      is_open_ = ofs_->is_open();
    }
    else
      is_open_ = true;
    return is_open_;
  }

  std::ostream& reference() const noexcept { return *(ofs_.get()); }

  ~Sync_ofstream_open_close()
  try {
    std::lock_guard guard{mutex_};
    if (ofs_->is_open())
      ofs_->close();
  }
  catch(...) {
  }
};

//------------------------------------------------------------------------------

//
class Flogger_sync : public Logger_wrap_sync {
  std::shared_ptr<Sync_ofstream_open_close> data_;

  void (Flogger_sync::*current_func)(const std::string& str);

  void open_file(const std::string& str)
  {
    if (!data_->is_open() && !data_->try_open())
      std::runtime_error("Cannot open file");

    write_after_open(str);
    current_func = &Flogger_sync::write_after_open;
  }

  void write_after_open(const std::string& str)
  {
    Logger_wrap_sync::write(str);
  }

public:
  explicit Flogger_sync(const std::shared_ptr<Sync_ofstream_open_close>& data,
    Lg_lvl ll = Lg_lvl::info)
    : Logger_wrap_sync{data->reference(), ll}, data_{data}
  {
    current_func = &Flogger_sync::open_file;
  }

  virtual void write(const std::string& str)
  {
    (this->*current_func) (str);
  }
};

//------------------------------------------------------------------------------
