/*
 * Copyright (c) 1993-1995 Argonaut Technologies Limited. All rights reserved.
 *
 * $Id: defdiag.c 1.1 1997/12/10 16:41:05 jon Exp $
 * $Locker: $
 *
 * Default diagnostic handler that does nothing
 */
#include "brender.h"

BR_RCS_ID("$Id: defdiag.c 1.1 1997/12/10 16:41:05 jon Exp $")

static void BrNullWarning(char *message)
{
}

static void BrNullFailure(char *message)
{
}

/*
 * DiagHandler structure
 */
br_diaghandler BrNullDiagHandler = {
	"Null DiagHandler",
	BrNullWarning,
	BrNullFailure,
};

/*
 * Global variable that can be overridden by linking something first
 * GCC and Clang do not allow for this behavior
 * To support this override with GCC and Clang the project has to be either
 * compiled with -fcommon (which would be global, also I have not tested this)
 * or the overrideable symbol has to be flagged as weak
 */
#ifdef __GNUC__
#define ATTRIBUTE_WEAK __attribute__((weak))
#else
#define ATTRIBUTE_WEAK
#endif
ATTRIBUTE_WEAK br_diaghandler *_BrDefaultDiagHandler = &BrNullDiagHandler;
