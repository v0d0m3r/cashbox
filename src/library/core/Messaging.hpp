#ifndef CASHBOX_MESSAGING_HPP
#define CASHBOX_MESSAGING_HPP

//------------------------------------------------------------------------------

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

inline std::mutex iom;

namespace messaging
{

struct message_base       // Base class of your queue entries
{
  virtual ~message_base()
  {}
};

template<typename Msg>
struct wrapped_message :  // Each message type has a specialization.
  message_base
{
  Msg contents;
  explicit wrapped_message(Msg const& contents_):
    contents(contents_)
  {}
};

// Listing C.1 a simple message queue
class queue // Your message queue
{
  std::mutex m;
  std::condition_variable c;
  std::queue<std::shared_ptr<message_base>> q;  // Internal queue stores
                                                // pointers to message_base
public:
  template<typename T>
  void push(T const& msg)
  {
    std::lock_guard<std::mutex> lk(m);
    q.push(std::make_shared<wrapped_message<T>>(msg)); // Wrap posted message and
                                                       // store pointer
    c.notify_all();
  }
  std::shared_ptr<message_base> wait_and_pop()
  {
    std::unique_lock<std::mutex> lk(m);
    c.wait(lk, [&]{return !q.empty();});    // Block until queue isn't empty
    auto res=q.front();
    q.pop();
    return res;
  }
};

// Listing C.5
template<typename PreviousDispatcher, typename Msg, typename Func>
class TemplateDispatcher
{
  queue* q;
  PreviousDispatcher* prev;
  Func f;
  bool chained;
  TemplateDispatcher(TemplateDispatcher const&) = delete;
  TemplateDispatcher& operator=(TemplateDispatcher const&) = delete;
  template<typename Dispatcher, typename OtherMsg, typename OtherFunc>
  friend class TemplateDispatcher;  // TemplateDispatcher instantiations are
                                    // friends of each other.
  void wait_and_dispatch()
  {
    for (;;)
    {
      auto msg = q->wait_and_pop();
      if (dispatch(msg))  // If you handle the message, break out of the loop.
        break;
    }
  }
  bool dispatch(std::shared_ptr<message_base> const& msg)
  {
    if (wrapped_message<Msg>* wrapper =                 // Check the message
        dynamic_cast<wrapped_message<Msg>*>(msg.get())) // type and call
    {                                                   // the function.
      f(wrapper->contents);
      return true;
    }
    else
    {
      return prev->dispatch(msg);  // Chain to the previous dispatcher.

    }
  }
public:
  TemplateDispatcher(TemplateDispatcher&& other):
    q(other.q), prev(other.prev), f(std::move(other.f)),
    chained(other.chained)
  {
    other.chained = true;
  }
  TemplateDispatcher(queue* q_, PreviousDispatcher* prev_, Func&& f_):
    q(q_), prev(prev_), f(std::forward<Func>(f_)), chained(false)
  {
    prev_->chained = true;
  }
  template<typename OtherMsg, typename OtherFunc>
  TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>
  handle(OtherFunc&& of)          // Additional handlers can be chained.
  {
    return TemplateDispatcher<
      TemplateDispatcher, OtherMsg, OtherFunc>(
      q, this, std::forward<OtherFunc>(of));
  }
  ~TemplateDispatcher() noexcept(false) // The destructor is noexcept(false)
  {                                     // again.
    if (!chained)
    {
      wait_and_dispatch();
    }
  }
};

// Listing C.4 The dispatcher
class close_queue // The message for closing the queue
{};

class dispatcher
{
  queue* q;
  bool chained;
  dispatcher(dispatcher const&) = delete;     // dispatcher instances cannot
                                              // be copied
  dispatcher& operator=(dispatcher const&) = delete;
  template<
    typename Dispatcher,
    typename Msg,
    typename Func>                  // Allow TemplateDispatcher instances to
  friend class TemplateDispatcher;  // access the internals.
  void wait_and_dispatch()
  {
    for (;;)      // Loop, waiting for, and dispatching messages
    {
      auto msg = q->wait_and_pop();
      dispatch(msg);
    }
  }
  bool dispatch(  // dispatch() checks for a close_queue message, and throws.
    std::shared_ptr<message_base> const& msg)
  {
    if (dynamic_cast<wrapped_message<close_queue>*>(msg.get()))
    {
      throw close_queue();
    }
    return false;
  }
public:
  dispatcher(dispatcher&& other): // Dispatcher instances can be moved.
    q(other.q), chained(other.chained)
  {
    other.chained=true;           // The source shouldn't
                                  // wait for messages.
  }
  explicit dispatcher(queue* q_):
    q(q_), chained(false)
  {}
  template<typename Message, typename Func>
  TemplateDispatcher<dispatcher, Message, Func>
  handle(Func&& f)                // Handle a specific type of message with a
  {                               // TemplateDispatcher.
    return TemplateDispatcher<dispatcher, Message, Func>(
      q, this, std::forward<Func>(f));
  }
  ~dispatcher() noexcept(false)   // The destructor might throw exceptions.
  {
    if (!chained)
    {
      wait_and_dispatch();
    }
  }
};

// Listing C.2 The sender class
class sender
{
  queue* q;         // sender is a wrapper around the queue pointer.
public:
  sender():         // Default-constructed sender has no queue
    q(nullptr)
  {}
  explicit sender (queue* q_) : // Allow construction from pointer to queue
    q(q_)
  {}
  template<typename Message>
  void send(Message const& msg)
  {
    if (q)
    {
      q->push(msg); // Sending pushes message on the queue
    }
  }
};

// Listing C.3 The receiver class
class receiver
{
  queue q;  // A receiver owns the queue.
public:
  operator sender()     // Allow implicit conversion to a sender
  {                     // that references the queue
    return sender(&q);
  }
  dispatcher wait()     // Waiting for a queue creates a dispatcher
  {
    return dispatcher(&q);
  }
};

}

//------------------------------------------------------------------------------

#endif // CASHBOX_MESSAGING_HPP
