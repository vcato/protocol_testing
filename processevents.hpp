#ifndef PROCESSEVENTS_HPP_
#define PROCESSEVENTS_HPP_

#include <vector>
#include "selector.hpp"


struct EventSinkInterface {
  virtual void setupSelect(PreSelectParamsInterface &) const = 0;
  virtual void handleSelect(const PostSelectParamsInterface &) = 0;
};


inline void
  processEvents(
    AbstractSelector &selector,
    const std::vector<EventSinkInterface *> &event_sinks
  )
{
  selector.beginSelect();

  for (auto event_sink_ptr : event_sinks) {
    assert(event_sink_ptr);
    event_sink_ptr->setupSelect(selector.preSelectParams());
  }

  selector.callSelect();

  for (auto event_sink_ptr : event_sinks) {
    assert(event_sink_ptr);
    event_sink_ptr->handleSelect(selector.postSelectParams());
  }

  selector.endSelect();
}


#endif /* PROCESSEVENTS_HPP_ */
