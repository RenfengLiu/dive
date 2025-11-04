#ifndef PPX_GRFX_VK_QC_EXTENSIONS_H
#define PPX_GRFX_VK_QC_EXTENSIONS_H

// Non-advertised private extension to enable direct rendering.
#define VK_QCOM_RENDER_MODE_CONTROL_EXTENSION_NAME "VK_QCOM_render_mode_control"

typedef enum VkRenderModeQCOM {
    VK_RENDER_MODE_OPTIMAL_QCOM = 0,
    VK_RENDER_MODE_FORCE_HW_VIS_BINNING_QCOM = 1,
    VK_RENDER_MODE_FORCE_HW_DIRECT_QCOM = 2,
} VkRenderModeQCOM;

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_MODE_CONTROL_FEATURES_QCOM 1000535000
#define VK_STRUCTURE_TYPE_RENDER_MODE_CONTROL_RENDER_PASS_BEGIN_INFO_QCOM 1000535001

// Provided by VK_QCOM_render_mode_control. pNext of VkRenderPassBeginInfo is
// to be set to this.
typedef struct VkRenderModeControlRenderPassBeginInfoQCOM {
    VkStructureType sType;
    const void* pNext;
    VkRenderModeQCOM preferredRenderMode;
} VkRenderModeControlRenderPassBeginInfoQCOM;

typedef struct VkPhysicalDeviceRenderModeControlFeaturesQCOM {
    VkStructureType sType;
    void* pNext;
    VkBool32 renderModeControl;
} VkPhysicalDeviceRenderModeControlFeaturesQCOM;

#endif
