// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CHILD_PROCESS_HOST_H_
#define CONTENT_PUBLIC_COMMON_CHILD_PROCESS_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/clang_profiling_buildflags.h"
#include "base/files/scoped_file.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class FilePath;
}

namespace IPC {
class MessageFilter;
}

namespace mojo {
class OutgoingInvitation;
}

namespace content {

class ChildProcessHostDelegate;

// This represents a non-browser process. This can include traditional child
// processes like plugins, or an embedder could even use this for long lived
// processes that run independent of the browser process.
class CONTENT_EXPORT ChildProcessHost : public IPC::Sender {
 public:
  ~ChildProcessHost() override {}

  // This is a value never returned as the unique id of any child processes of
  // any kind, including the values returned by RenderProcessHost::GetID().
  enum : int { kInvalidUniqueID = -1 };

  // Every ChildProcessHost provides a single primordial Mojo message pipe to
  // the launched child process, with the other end held by the
  // ChildProcessHost.
  //
  // This enum (given to |Create()|) determines how the ChildProcessHost uses
  // the pipe.
  enum class IpcMode {
    // In this mode, the primordial pipe is a content.mojom.ChildProcess pipe.
    // The ChildProcessHost is fully functional in this mode, and all new
    // process hosts should prefer to use this mode.
    kNormal,

    // In this mode, the primordial pipe is a legacy IPC Channel bootstrapping
    // pipe (IPC.mojom.ChannelBootstrap). This should be used when the child
    // process only uses legacy Chrome IPC (e.g. Chrome's NaCl processes.)
    //
    // In this mode, ChildProcessHost methods like |BindReceiver()| are not
    // functional.
    //
    // DEPRECATED: Do not introduce new uses of this mode.
    kLegacy,
  };

  // Used to create a child process host. The delegate must outlive this object.
  static std::unique_ptr<ChildProcessHost> Create(
      ChildProcessHostDelegate* delegate,
      IpcMode ipc_mode);

  // These flags may be passed to GetChildPath in order to alter its behavior,
  // causing it to return a child path more suited to a specific task.
  enum {
    // No special behavior requested.
    CHILD_NORMAL = 0,

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_HAIKU)
    // Indicates that the child execed after forking may be execced from
    // /proc/self/exe rather than using the "real" app path. This prevents
    // autoupdate from confusing us if it changes the file out from under us.
    // You will generally want to set this on Linux, except when there is an
    // override to the command line (for example, we're forking a renderer in
    // gdb). In this case, you'd use GetChildPath to get the real executable
    // file name, and then prepend the GDB command to the command line.
    CHILD_ALLOW_SELF = 1 << 0,
#elif defined(OS_MAC)
    // Note, on macOS these are not bitwise flags and each value is mutually
    // exclusive with the others. Each one of these options should correspond
    // to a value in //content/public/app/mac_helpers.gni.

    // Starts a child process with the macOS entitlement that allows JIT (i.e.
    // memory that is writable and executable). In order to make use of this,
    // memory cannot simply be allocated as read-write-execute; instead, the
    // MAP_JIT flag must be passed to mmap() when allocating the memory region
    // into which the writable-and-executable data are stored.
    CHILD_RENDERER,

    // Starts a child process with the macOS entitlement that allows unsigned
    // executable memory.
    // TODO(https://crbug.com/985816): Change this to use MAP_JIT and the
    // allow-jit entitlement instead.
    CHILD_GPU,

    // Starts a child process with the macOS entitlement that ignores the
    // library validation code signing enforcement. Library validation mandates
    // that all executable pages be backed by a code signature that either 1)
    // is signed by Apple, or 2) signed by the same Team ID as the main
    // executable. Binary plug-ins that are not always signed by the same Team
    // ID as the main binary, so this flag should be used when needing to load
    // third-party plug-ins.
    CHILD_PLUGIN,

#if defined(ARCH_CPU_ARM64)
    // Launch the child process as CHILD_NORMAL, but as x86_64 code under
    // Rosetta translation. The executable being launched must contain x86_64
    // code, either as a thin Mach-O file targeting x86_64, or a fat file with
    // an x86_64 slice. Aside from the architecture, semantics are identical to
    // CHILD_NORMAL, and this cannot be combined with any other CHILD_* values.
    CHILD_LAUNCH_X86_64,
#endif  // ARCH_CPU_ARM64
#endif
  };

  // Returns the pathname to be used for a child process.  If a subprocess
  // pathname was specified on the command line, that will be used.  Otherwise,
  // the default child process pathname will be returned.  On most platforms,
  // this will be the same as the currently-executing process.
  //
  // The |flags| argument accepts one or more flags such as CHILD_ALLOW_SELF.
  // Pass only CHILD_NORMAL if none of these special behaviors are required.
  //
  // On failure, returns an empty FilePath.
  static base::FilePath GetChildPath(int flags);

  // Send the shutdown message to the child process.
  virtual void ForceShutdown() = 0;

  // Exposes the outgoing Mojo invitation for this ChildProcessHost. The
  // invitation can be given to ChildProcessLauncher to ensure that this
  // ChildProcessHost's primordial Mojo IPC calls can properly communicate with
  // the launched process.
  //
  // Always valid immediately after ChildProcessHost construction, but may be
  // null if someone else has taken ownership.
  virtual base::Optional<mojo::OutgoingInvitation>& GetMojoInvitation() = 0;

  // Creates the IPC channel over a Mojo message pipe. The pipe connection is
  // brokered through the Service Manager like any other service connection.
  virtual void CreateChannelMojo() = 0;

  // Returns true iff the IPC channel is currently being opened;
  virtual bool IsChannelOpening() = 0;

  // Adds an IPC message filter.  A reference will be kept to the filter.
  virtual void AddFilter(IPC::MessageFilter* filter) = 0;

  // Bind an interface exposed by the child process. Whether or not the
  // interface in |receiver| can be bound depends on the process type and
  // potentially on the Content embedder.
  //
  // Receivers passed to this call arrive in the child process and go through
  // the following flow, stopping if any step decides to bind the receiver:
  //
  //   1. IO thread, ChildProcessImpl::BindReceiver.
  //   2. IO thread, ContentClient::BindChildProcessInterface.
  //   3. Main thread, ChildThreadImpl::BindReceiver (virtual).
  virtual void BindReceiver(mojo::GenericPendingReceiver receiver) = 0;

  // Instructs the child process to run an instance of the named service.
  virtual void RunService(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver) = 0;

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  // Write out the accumulated code profiling profile to the configured file.
  // The callback is invoked once the profile has been flushed to disk.
  virtual void DumpProfilingData(base::OnceClosure callback) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CHILD_PROCESS_HOST_H_
