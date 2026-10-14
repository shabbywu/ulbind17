#pragma once
#include <Ultralight/js/API.h>
#include <Ultralight/js/Class.h>
#include <algorithm>
#include <ulbind17/runtime.hpp>
#include <ulbind17/values.hpp>

namespace ulbind17 {
// A value-type SDK API with checked ManagedView attachment. No polymorphic ownership.
// Keep an owning API handle alive while its bindings are needed. SDK borrowed refs
// stored by ManagedView do not keep that owner alive.
class API : public js::API {
  public:
    using js::API::API;
    using js::API::AttachTo;
    using js::API::DetachFrom;

    [[nodiscard]] bool AttachTo(const ManagedView &managed, const js::AttachOptions &options = {}) {
        auto state = managed.state_;
        state->require_open();
        state->require_unlocked();
        auto runtime = state->runtime;
        auto host = runtime->host_keepalive;
        auto view = state->view;
        if (!raw())
            return false;
        auto &apis = state->attached_apis;
        const bool tracked = std::any_of(apis.begin(), apis.end(), [&](const auto &api) { return api.raw() == raw(); });
        // Allocate before changing native attachment, so allocation failure is atomic.
        auto borrowed = js::API::FromBorrowed(raw());
        if (!tracked)
            apis.reserve(apis.size() + 1);
        if (!js::API::AttachTo(view.get(), options))
            return false;
        if (state->closed || runtime->closed) {
            borrowed.DetachFrom(view.get());
            throw Error(ErrorCode::Closed, "View closed during API attachment");
        }
        if (!tracked)
            apis.push_back(std::move(borrowed));
        return true;
    }

    void DetachFrom(const ManagedView &managed) {
        auto state = managed.state_;
        state->require_open();
        state->require_unlocked();
        auto runtime = state->runtime;
        auto host = runtime->host_keepalive;
        auto view = state->view;
        js::API::DetachFrom(view.get());
        std::erase_if(state->attached_apis, [&](const auto &api) { return api.raw() == raw(); });
    }
};
} // namespace ulbind17
