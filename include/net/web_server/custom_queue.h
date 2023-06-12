#pragma once

#include <vector>
#include <memory>

#include "net/web_server/pending_request.h"

// This queue is used for HTTP pipelining.
template<typename http_session>
struct queue {
  // The type-erased, saved work item
  struct response {
    response() = default;
    virtual ~response() = default;
    response(response const&) = delete;
    response& operator=(response const&) = delete;
    response(response&&) = delete;
    response& operator=(response&&) = delete;

    virtual void send() = 0;
  };

  typedef pending_request<http_session, response> pending_request2;

  explicit queue(http_session& self, std::size_t limit)
      : self_(self), limit_(limit) {
    items_.reserve(limit);
  }

  ~queue() = default;
  queue(queue const&) = delete;
  queue& operator=(queue const&) = delete;
  queue(queue&&) = delete;
  queue& operator=(queue&&) = delete;

  // Returns `true` if we have reached the queue limit
  bool is_full() const { return items_.size() >= limit_; }

  // Called when a message finishes sending
  // Returns `true` if the caller should initiate a read
  bool on_write() {
    BOOST_ASSERT(!items_.empty());
    auto const was_full = is_full();
    items_.erase(items_.begin());
    return was_full;
  }

  bool send_next() {
    if (items_.empty() || !items_.front()->is_finished()) {
      return false;
    }
    self_.write_active_ = true;
    items_.front()->response_->send();
    return true;
  }

  pending_request2& add_entry() {
    return *items_.emplace_back(std::make_unique<pending_request2>(self_))
                .get();
  }

  http_session& self_;
  std::vector<std::unique_ptr<pending_request2>> items_;
  // Maximum number of responses we will queue
  std::size_t limit_;
};