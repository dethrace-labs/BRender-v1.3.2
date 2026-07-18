#include "drv.h"

void VK_AssertOrWarnIfError(VkResult result, char *file, int line, const char *expr) {
    const char *kind;
    switch (result) {
        case VK_SUCCESS: return;
        case VK_NOT_READY:            kind = "VK_NOT_READY"; break;
        case VK_TIMEOUT:              kind = "VK_TIMEOUT"; break;
        case VK_EVENT_SET:            kind = "VK_EVENT_SET"; break;
        case VK_EVENT_RESET:          kind = "VK_EVENT_RESET"; break;
        case VK_INCOMPLETE:           kind = "VK_INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:    kind = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:  kind = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED: kind = "VK_ERROR_INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST:           kind = "VK_ERROR_DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED:     kind = "VK_ERROR_MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT:     kind = "VK_ERROR_LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT: kind = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_FEATURE_NOT_PRESENT:   kind = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:   kind = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
        case VK_ERROR_TOO_MANY_OBJECTS:      kind = "VK_ERROR_TOO_MANY_OBJECTS"; break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:  kind = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
        case VK_ERROR_SURFACE_LOST_KHR:      kind = "VK_ERROR_SURFACE_LOST_KHR"; break;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: kind = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
        case VK_SUBOPTIMAL_KHR:              kind = "VK_SUBOPTIMAL_KHR"; break;
        case VK_ERROR_OUT_OF_DATE_KHR:       kind = "VK_ERROR_OUT_OF_DATE_KHR"; break;
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: kind = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
        default:                           kind = "<unknown>"; break;
    }
    BrWarning("Vulkan error: %s:%d %s (0x%x) - %s\n", file, line, kind, (int)result, expr);
}
