// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/evproto2
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#ifndef EVPROTO2_EVENTLOOP_H
#define EVPROTO2_EVENTLOOP_H

#include <event2/event.h>

namespace evproto
{

class EventLoop // : boost::noncopyable
{
 public:
  EventLoop()
    : base_(::event_base_new())
  {
    assert(base_ != NULL);
  }

  ~EventLoop()
  {
    ::event_base_free(base_);
  }

  int loop()
  {
    return ::event_base_loop(base_, 0);
  }

  struct event_base* eventBase()
  {
    return base_;
  }

 private:
  struct event_base* const base_;
  // pthread_t 

  void operator=(const EventLoop&);
  EventLoop(const EventLoop&);
};
}

#endif  // EVPROTO2_EVENTLOOP_H

