// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "base/macros.h"

#if defined(OS_ANDROID)
#include "third_party/blink/public/mojom/font_unique_name_lookup/font_unique_name_lookup.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/android/font_unique_name_lookup_android.h"
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_HAIKU)
#include "third_party/blink/renderer/platform/fonts/linux/font_unique_name_lookup_linux.h"
#elif defined(OS_WIN)
#include "third_party/blink/renderer/platform/fonts/win/font_unique_name_lookup_win.h"
#endif

namespace blink {

FontUniqueNameLookup::FontUniqueNameLookup() = default;

// static
std::unique_ptr<FontUniqueNameLookup>
FontUniqueNameLookup::GetPlatformUniqueNameLookup() {
#if defined(OS_ANDROID)
  return std::make_unique<FontUniqueNameLookupAndroid>();
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_HAIKU)
  return std::make_unique<FontUniqueNameLookupLinux>();
#elif defined(OS_WIN)
  return std::make_unique<FontUniqueNameLookupWin>();
#else
  return nullptr;
#endif
}

}  // namespace blink
