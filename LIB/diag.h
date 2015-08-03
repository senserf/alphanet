#ifndef __diag_h__
#define __diag_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014.                   */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "sysio.h"

// level defs
#define D_OFF		0
#define D_UI		2
#define D_FATAL		4
#define D_SERIOUS	6
#define D_WARNING	8
#define D_INFO		10
#define D_DEBUG		12
#define D_ALL		42

// current levels
#define APP_DL		D_WARNING

// compile out what's not needed
#if APP_DL < D_DEBUG
#define app_diag_D(...) CNOP
#else
#define app_diag_D(...) _app_diag(__VA_ARGS__)
#endif

#if APP_DL < D_INFO
#define app_diag_I(...) CNOP
#else
#define app_diag_I(...) _app_diag(__VA_ARGS__)
#endif

#if APP_DL < D_WARNING
#define app_diag_W(...) CNOP
#else
#define app_diag_W(...) _app_diag(__VA_ARGS__)
#endif

#if APP_DL < D_SERIOUS
#define app_diag_S(...) CNOP
#else
#define app_diag_S(...) _app_diag(__VA_ARGS__)
#endif

#if APP_DL < D_FATAL
#define app_diag_F(...) CNOP
#else
#define app_diag_F(...) _app_diag(__VA_ARGS__)
#endif

#if APP_DL < D_UI
#define app_diag_U(...) CNOP
#else
#define app_diag_U(...) _app_diag(__VA_ARGS__)
#endif

void  _app_diag (const char *, ...);

//+++ diag.cc
#endif
