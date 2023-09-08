#ifndef CASHBOX_MESSAGING_HPP
#define CASHBOX_MESSAGING_HPP

//------------------------------------------------------------------------------

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

//------------------------------------------------------------------------------

inline std::mutex iom;

//------------------------------------------------------------------------------

namespace Messaging {

//------------------------------------------------------------------------------

struct Message_base { // Base class of your queue entries
  virtual ~Message_base() = default;
};

//------------------------------------------------------------------------------

template<class Msg>   // Each message type has a specialization.
struct Wrapped_message : Message_base {

  explicit Wrapped_message(const Msg& contents) : contents_(contents) {}

  const auto& contents() const noexcept { return contents_; }

private:
  Msg contents_;
};

//------------------------------------------------------------------------------

class Simple_queue {
  std::mutex m_;
  std::condition_variable cv_;
  std::queue<std::shared_ptr<Message_base>> q_; // Internal queue stores
                                                // pointers to message_base
public:
  template<class T>
  void push(T&& msg)
  {
    std::lock_guard lk{m_};
    // Wrap posted message and store pointer
    q_.push(std::make_shared<Wrapped_message<T>>(std::forward<T>(msg)));
    cv_.notify_all();
  }

  auto wait_and_pop()
  {
    std::unique_lock lk{m_};
    cv_.wait(lk, [&] { return !q_.empty(); }); // Block until queue isn't empty
    auto res{q_.front()};
    q_.pop();
    return res;
  }
};

//------------------------------------------------------------------------------

template<class Previous_dispatcher, class Msg, class Func>
class Template_dispatcher {
  Simple_queue*const q_;
  Previous_dispatcher*const prev_;
  Func f_;
  bool chained_;

  template<class Other_dispatcher, class Other_msg, class Other_func>
  friend class Template_dispatcher; // Template_dispatcher instantiations are
                                    // friends of each other.
  void wait_and_dispatch()
  {
    for (;;)
      if (dispatch(q_->wait_and_pop()))
        break;                      // If you handle the message,
  }                                 // break out of the loop.

  bool dispatch(const std::shared_ptr<Message_base>& msg)
  {
    if (auto*const wrapper =                              // Check the message
        dynamic_cast<Wrapped_message<Msg>*>(msg.get())) { // type and call
                                                          // the function.
      f_(wrapper->contents());
      return true;
    }
    return prev_->dispatch(msg);    // Chain to the previous dispatcher.
  }
public:
  Template_dispatcher(Simple_queue*const q, Previous_dispatcher*const prev, Func&& f):
    q_{q}, prev_{prev}, f_{std::move(f)}, chained_{false}
  { prev_->chained_ = true; }

  Template_dispatcher(Simple_queue*const q, Previous_dispatcher*const prev, const Func& f):
    q_{q}, prev_{prev}, f_{f}, chained_{false}
  { prev_->chained_ = true; }

  Template_dispatcher(Template_dispatcher const&) = delete;
  Template_dispatcher& operator=(Template_dispatcher const&) = delete;

  Template_dispatcher(Template_dispatcher&& other) :
    q_{other.q_}, prev_{other.prev_}, f_{std::move(other.f_)},
    chained_{other.chained_}
  { other.chained_ = true; }

  template<class Other_msg, class Other_func>
  Template_dispatcher<Template_dispatcher, Other_msg, Other_func>
  handle(Other_func&& of)          // Additional handlers can be chained.
  {
    return Template_dispatcher<Template_dispatcher, Other_msg, Other_func>(
      q_, this, std::forward<Other_func>(of));
  }

  ~Template_dispatcher() noexcept(false)
  {
    if (!chained_)
      wait_and_dispatch();
  }
};

//------------------------------------------------------------------------------

class Close_queue {}; // The message for closing the queue

//------------------------------------------------------------------------------

class Dispatcher {
  Simple_queue*const q_;
  bool chained_;

  template<class Other_dispatcher, class Msg, class Func>
  friend class Template_dispatcher; // Allow Template_dispatcher instances to
                                    // access the internals.
  void wait_and_dispatch()
  {
    for (;;)  // Loop, waiting for, and dispatching messages
      dispatch(q_->wait_and_pop());
  }

  bool dispatch(  // dispatch() checks for a close_queue message, and throws.
    const std::shared_ptr<Message_base>& msg)
  {
    if (dynamic_cast<Wrapped_message<Close_queue>*>(msg.get()))
      throw Close_queue();
    return false;
  }
public:

  explicit Dispatcher(Simple_queue*const q) : q_{q}, chained_{false} {}

  Dispatcher(Dispatcher const&) = delete;
  Dispatcher& operator=(Dispatcher const&) = delete;

  Dispatcher(Dispatcher&& other) :
    q_{other.q_}, chained_{other.chained_}
  {
    other.chained_ = true; // The source shouldn't
                           // wait for messages.
  }

  template<class Message, class Func>
  Template_dispatcher<Dispatcher, Message, Func>
  handle(Func&& f)                // Handle a specific type of message with a
  {                               // Template_dispatcher.
    return Template_dispatcher<Dispatcher, Message, Func>(
      q_, this, std::forward<Func>(f));
  }

  ~Dispatcher() noexcept(false)   // The destructor might throw exceptions.
  {
    if (!chained_)
      wait_and_dispatch();
  }
};

//------------------------------------------------------------------------------

class Sender {
  Simple_queue* q_;         // sender is a wrapper around the queue pointer.
public:
  Sender() : q_{nullptr} {}

  explicit Sender(Simple_queue*const q) : q_{q} {}

  template<class Message>
  void send(const Message& msg)
  {
    if (q_)
      q_->push(msg);      // Sending pushes message on the queue
  }
};

//------------------------------------------------------------------------------

class Receiver {
  Simple_queue q_;      // A receiver owns the queue.
public:
  operator Sender()     // Allow implicit conversion to a sender
  {                     // that references the queue
    return Sender(&q_);
  }
  Dispatcher wait()     // Waiting for a queue creates a dispatcher
    { return Dispatcher(&q_); }
};

//------------------------------------------------------------------------------

}

//------------------------------------------------------------------------------

#endif // CASHBOX_MESSAGING_HPP
