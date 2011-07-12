LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    Layer.cpp 								\
    LayerBase.cpp 							\
    LayerDim.cpp 							\
    DisplayHardware/DisplayHardware.cpp 	\
    DisplayHardware/DisplayHardwareBase.cpp \
    DisplayHardware/HWComposer.cpp 			\
    GLExtensions.cpp 						\
    MessageQueue.cpp 						\
    SurfaceFlinger.cpp 						\
    SurfaceTextureLayer.cpp 				\
    Transform.cpp 							\
    

LOCAL_CFLAGS:= -DLOG_TAG=\"SurfaceFlinger\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

ifeq ($(TARGET_BOARD_PLATFORM), omap3)
	LOCAL_CFLAGS += -DNO_RGBX_8888
endif
ifeq ($(TARGET_BOARD_PLATFORM), s5pc110)
	LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY
endif


LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libhardware \
	libutils \
	libEGL \
	libGLESv1_CM \
	libbinder \
	libui \
	libgui

LOCAL_C_INCLUDES := \
	$(call include-path-for, corecg graphics)

LOCAL_C_INCLUDES += hardware/libhardware/modules/gralloc

LOCAL_MODULE:= libsurfaceflinger

include $(BUILD_SHARED_LIBRARY)
