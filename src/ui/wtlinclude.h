#pragma once

// Shared WTL/ATL preamble for the UI library. Every translation unit that uses
// CWindowImpl-derived controls must include this header first. The application
// defines the single global _Module instance in main.cpp.

#include <atlbase.h>
#include <atlapp.h>
extern CAppModule _Module;
#include <atlctrls.h>
#include <atlwin.h>
