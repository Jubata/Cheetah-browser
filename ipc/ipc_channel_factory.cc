// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_channel_mojo.h"

namespace IPC {

namespace {

class PlatformChannelFactory : public ChannelFactory {
 public:
  PlatformChannelFactory(
      ChannelHandle handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
      : handle_(handle), mode_(mode), ipc_task_runner_(ipc_task_runner) {}

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
#if defined(OS_NACL_SFI)
    return Channel::Create(handle_, mode_, listener);
#else
    DCHECK(handle_.is_mojo_channel_handle());
    return ChannelMojo::Create(
        mojo::ScopedMessagePipeHandle(handle_.mojo_handle), mode_, listener,
        ipc_task_runner_);
#endif
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() override {
    return ipc_task_runner_;
  }

 private:
  ChannelHandle handle_;
  Channel::Mode mode_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannelFactory);
};

} // namespace

// static
std::unique_ptr<ChannelFactory> ChannelFactory::Create(
    const ChannelHandle& handle,
    Channel::Mode mode,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  return std::make_unique<PlatformChannelFactory>(handle, mode,
                                                  ipc_task_runner);
}

}  // namespace IPC
