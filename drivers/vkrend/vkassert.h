#include "drv.h"

#ifndef VK_ASSERT_H
#define VK_ASSERT_H

void VK_AssertOrWarnIfError(VkResult result, char *file, int line, const char *expr);

#endif
