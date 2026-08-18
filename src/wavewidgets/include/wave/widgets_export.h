#pragma once

#if defined(_WIN32)
#  if defined(wavewidgets_EXPORTS)
#    define WAVEWIDGETS_API __declspec(dllexport)
#  else
#    define WAVEWIDGETS_API __declspec(dllimport)
#  endif
#else
#  define WAVEWIDGETS_API __attribute__((visibility("default")))
#endif
