#pragma once
#include <ulbind17/Error.hpp>
#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace ulbind17::jsc {
// One traversal per conversion. Only the current path is tracked: repeated siblings
// are allowed. The caller supplies JS object or host container identity.
class Traversal {
    std::vector<const void *> path_;
    std::size_t limit_;
  public:
    explicit Traversal(std::size_t limit = 64) : limit_(limit) {}
    class Guard {
        Traversal *owner_;
      public:
        Guard(Traversal &owner, const void *identity) : owner_(&owner) {
            if (!identity) throw Error(ErrorCode::InvalidArgument, "Empty traversal identity");
            if (owner.path_.size() >= owner.limit_ ||
                std::find(owner.path_.begin(), owner.path_.end(), identity) != owner.path_.end())
                throw Error(ErrorCode::TraversalLimit, "Cyclic or excessively nested data");
            owner.path_.push_back(identity);
        }
        ~Guard() { if (owner_) owner_->path_.pop_back(); }
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
        Guard(Guard &&other) noexcept : owner_(std::exchange(other.owner_, nullptr)) {}
    };
    [[nodiscard]] Guard enter(const void *identity) { return Guard(*this, identity); }
};
} // namespace ulbind17::jsc
