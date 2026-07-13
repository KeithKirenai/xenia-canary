/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/apps/xam_app.h"

#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xam/xam_content_device.h"
#include "xenia/kernel/xenumerator.h"
#include "xenia/hid/kinect/kinect_input_driver.h"
#include "xenia/base/memory.h"
#include "xenia/kernel/xthread.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/kernel/xevent.h"
#include "xenia/kernel/xsemaphore.h"

static xe::hid::kinect::KinectInputDriver* kd() {
  return xe::hid::kinect::KinectInputDriver::instance();
}

static void NuiCOMStub_QueryInterface(xe::cpu::ppc::PPCContext* context) {
  uint32_t ppvObject = static_cast<uint32_t>(context->r[5]);
  if (ppvObject) {
    auto* ppv_val = context->TranslateVirtual<uint32_t*>(ppvObject);
    if (ppv_val) {
      xe::store_and_swap<uint32_t>(ppv_val, static_cast<uint32_t>(context->r[3]));
      XELOGI("NuiCOMStub_QueryInterface: wrote interface ptr {:08X} to ppvObject {:08X}",
             static_cast<uint32_t>(context->r[3]), ppvObject);
    }
  }
  context->r[3] = 0; // S_OK
}

static int g_nui_com_call_count = 0;
static void NuiCOMStub_Success(xe::cpu::ppc::PPCContext* context) {
  uint32_t this_ptr = static_cast<uint32_t>(context->r[3]);
  uint32_t method_id = g_nui_com_call_count++;
  if (method_id < 5) {
    XELOGI("NuiCOMStub_Success: call #{:d} this={:08X} r3={:08X} r4={:08X}",
           method_id, this_ptr,
           static_cast<uint32_t>(context->r[3]),
           static_cast<uint32_t>(context->r[4]));
  }
  context->r[3] = 0; // S_OK / 0
}

/* Notes:
   - Messages ids that start with 0x00021xxx are UI calls
   - Messages ids that start with 0x00023xxx are used for the user profile
   - Messages ids that start with 0x0002Bxxx are used by the Kinect device
   usually for camera related functions
   - Messages ids that start with 0x0002Cxxx are used by the XamNuiIdentity
   functions
*/

namespace xe {
namespace kernel {
namespace xam {
namespace apps {

XamApp::XamApp(KernelState* kernel_state) : App(kernel_state, 0xFE) {}

X_HRESULT XamApp::DispatchMessageSync(uint32_t message, uint32_t buffer_ptr,
                                      uint32_t buffer_length) {
  // NOTE: buffer_length may be zero or valid.
  auto buffer = memory_->TranslateVirtual(buffer_ptr);
  switch (message) {
    case 0x0002000E: {
      X_ENUMERATE_PARAM* data_ptr =
          reinterpret_cast<X_ENUMERATE_PARAM*>(buffer);

      XELOGD(
          "XEnumerateCrossTitle({:04X}, {:04X}, {:04X}, {:04X}, {}, {}, "
          "{:04X})",
          data_ptr->user_index.get(), data_ptr->flags.get(),
          data_ptr->private_enum_structure_ptr.get(),
          data_ptr->buffer_ptr.get(), data_ptr->buffer_size.get(),
          data_ptr->items_requested.get(), data_ptr->items_returned_ptr.get());

      if (!data_ptr->buffer_ptr || !data_ptr->private_enum_structure_ptr) {
        return X_E_INVALIDARG;
      }

      auto enum_struct =
          memory_->TranslateVirtual<X_KENUMERATOR_CONTENT_AGGREGATE*>(
              data_ptr->private_enum_structure_ptr);

      auto e = kernel_state_->object_table()->LookupObject<XEnumerator>(
          enum_struct->handle);

      if (!e) {
        return X_E_INVALIDARG;
      }

      assert_true(enum_struct->magic == kXObjSignature);

      XCONTENT_CROSS_TITLE_DATA cross_title_data = {};
      uint8_t* cross_title_data_ptr =
          reinterpret_cast<uint8_t*>(&cross_title_data);

      uint32_t item_count = 0;
      X_RESULT result = e->WriteItems(cross_title_data_ptr,
                                      data_ptr->buffer_size, &item_count);

      XCONTENT_DATA_INTERNAL* content_data_ptr =
          memory_->TranslateVirtual<XCONTENT_DATA_INTERNAL*>(
              data_ptr->buffer_ptr);

      assert_true(data_ptr->buffer_size == sizeof(XCONTENT_DATA_INTERNAL));

      std::memset(content_data_ptr, 0, data_ptr->buffer_size);

      if (!result) {
        content_data_ptr->device_id = cross_title_data.content_data.device_id;
        content_data_ptr->content_type =
            cross_title_data.content_data.content_type;
        content_data_ptr->set_display_name(
            cross_title_data.content_data.display_name());
        content_data_ptr->set_file_name(
            cross_title_data.content_data.file_name());
        content_data_ptr->padding[0] = content_data_ptr->padding[1] = 0;
        content_data_ptr->title_id = cross_title_data.title_id;
      }

      result = X_HRESULT_FROM_WIN32(result);

      xe::be<uint32_t>* items_returned_ptr =
          memory_->TranslateVirtual<xe::be<uint32_t>*>(
              data_ptr->items_returned_ptr);

      *items_returned_ptr = item_count;

      return result;
    }
    case 0x00020021: {
      struct XContentQueryVolumeDeviceType {
        char root_name[64];
        xe::be<uint32_t> is_title_process;
        xe::be<DeviceType> device_type_ptr;
        xe::be<uint32_t> overlapped_ptr;
      }* data = reinterpret_cast<XContentQueryVolumeDeviceType*>(buffer);
      assert_true(buffer_length == sizeof(XContentQueryVolumeDeviceType));

      std::string target;
      if (!kernel_state_->file_system()->FindSymbolicLink(
              std::string(data->root_name) + ':', target)) {
        return X_E_INVALIDARG;
      }

      // Only apply this check to XContent packages
      if (!target.starts_with("\\Device\\Package_")) {
        return X_E_INVALIDARG;
      }

      xe::be<DeviceType>* device_type_ptr =
          memory_->TranslateVirtual<xe::be<DeviceType>*>(
              static_cast<uint32_t>(data->device_type_ptr.get()));

      switch (kernel_state_->deployment_type_) {
        case XDeploymentType::kDownload:
        case XDeploymentType::kInstalledToHDD: {
          *device_type_ptr = DeviceType::HDD;
        } break;
        case XDeploymentType::kOpticalDisc: {
          *device_type_ptr = DeviceType::ODD;
        } break;
        default: {
          *device_type_ptr = DeviceType::Invalid;
        } break;
      }

      XELOGD("XContentQueryVolumeDeviceType('{}', {:08X}, {:08X}, {:08X})",
             data->root_name,
             static_cast<uint32_t>(data->is_title_process.get()),
             static_cast<uint32_t>(data->device_type_ptr.get()),
             static_cast<uint32_t>(data->overlapped_ptr.get()));

      return X_E_SUCCESS;
    }
    case 0x00021012: {
      uint32_t enabled = xe::load_and_swap<uint32_t>(buffer);
      XELOGD("XEnableGuestSignin: {}", enabled ? "true" : "false");
      return X_E_SUCCESS;
    }
    case 0x00022005: {
      struct XTITLE_GET_DEPLOYMENT_TYPE {
        xe::be<uint32_t> deployment_type_ptr;
        xe::be<uint32_t> overlapped_ptr;
      }* data = reinterpret_cast<XTITLE_GET_DEPLOYMENT_TYPE*>(buffer);
      assert_true(!buffer_length ||
                  buffer_length == sizeof(XTITLE_GET_DEPLOYMENT_TYPE));
      auto deployment_type =
          memory_->TranslateVirtual<uint32_t*>(data->deployment_type_ptr);
      *deployment_type = static_cast<uint32_t>(kernel_state_->deployment_type_);
      XELOGD("XTitleGetDeploymentType({:08X}, {:08X}",
             data->deployment_type_ptr.get(), data->overlapped_ptr.get());
      return X_E_SUCCESS;
    }
    case 0x0002B001: {
      // NUI device initialization via XMsgInProcessCall.
      // Allocate device + vtable from guest heap ONLY ONCE, then reuse.
      // The game calls this repeatedly; leaking each time exhausts the heap.
      static uint32_t cached_device_addr = 0;
      static uint32_t cached_vtable_addr = 0;
      static bool nui_stubs_registered = false;

      if (buffer_ptr) {
        auto* out = memory_->TranslateVirtual<uint32_t*>(buffer_ptr);
        if (out) {
          XELOGI("XamApp: 0x2B001 raw buffer content: {:08X} {:08X} {:08X} {:08X} "
                 "{:08X} {:08X} {:08X} {:08X}",
                 xe::byte_swap(out[0]), xe::byte_swap(out[1]),
                 xe::byte_swap(out[2]), xe::byte_swap(out[3]),
                 xe::byte_swap(out[4]), xe::byte_swap(out[5]),
                 xe::byte_swap(out[6]), xe::byte_swap(out[7]));

          uint32_t original_handle = xe::byte_swap(out[0]);
          uint32_t status_ptr = xe::byte_swap(out[1]);
          uint32_t hr_ptr = xe::byte_swap(out[2]);

          if (kd() && !kd()->is_initialized()) {
            kd()->NuiInitialize(0x08);
          }

          // Allocate guest memory for mock NUI device only on first call:
          if (!cached_device_addr) {
            cached_device_addr = memory_->SystemHeapAlloc(4);
            cached_vtable_addr = memory_->SystemHeapAlloc(128);

            if (cached_device_addr && cached_vtable_addr) {
              // Register COM stubs once:
              if (!nui_stubs_registered) {
                uint32_t qi_trampoline =
                    kernel_state_->kernel_trampoline_group()->NewLongtermTrampoline(
                        NuiCOMStub_QueryInterface);
                uint32_t success_trampoline =
                    kernel_state_->kernel_trampoline_group()->NewLongtermTrampoline(
                        NuiCOMStub_Success);

                auto* vtable_data =
                    memory_->TranslateVirtual<uint32_t*>(cached_vtable_addr);
                if (vtable_data) {
                  xe::store_and_swap<uint32_t>(&vtable_data[0], qi_trampoline);
                  for (int i = 1; i < 32; i++) {
                    xe::store_and_swap<uint32_t>(&vtable_data[i],
                                                 success_trampoline);
                  }
                }

                auto* device_data =
                    memory_->TranslateVirtual<uint32_t*>(cached_device_addr);
                if (device_data) {
                  xe::store_and_swap<uint32_t>(device_data, cached_vtable_addr);
                }
                nui_stubs_registered = true;
              }
              XELOGI("XamApp: 0x2B001 mock device allocated: device={:08X}, vtable={:08X}",
                     cached_device_addr, cached_vtable_addr);
            } else {
              XELOGE("XamApp: 0x2B001 SystemHeapAlloc FAILED (device={:08X}, vtable={:08X})",
                     cached_device_addr, cached_vtable_addr);
              cached_device_addr = 0;
              cached_vtable_addr = 0;
            }
          }

          // Signal the initialization event (every call):
          if (original_handle) {
            auto object =
                kernel_state_->object_table()->LookupObject<XObject>(original_handle);
            if (!object && original_handle == 0x3E8) {
              auto ev = object_ref<XEvent>(new XEvent(kernel_state_));
              ev->Initialize(true, false);
              kernel_state_->object_table()->RestoreHandle(original_handle, ev.get());
              object = ev;
            }
            if (object) {
              if (object->type() == XObject::Type::Event) {
                auto ev = static_cast<xe::kernel::XEvent*>(object.get());
                ev->Set(0, false);
                XELOGI("XamApp: 0x2B001 signaled Event handle {:08X}",
                       original_handle);
              } else if (object->type() == XObject::Type::Semaphore) {
                auto sem = static_cast<xe::kernel::XSemaphore*>(object.get());
                std::ignore = sem->ReleaseSemaphore(1, nullptr);
              }
            }
          }

          // Ghidra revealed DAT_9251c3a0 is a C++ NUI object:
          //   +0x00: vtable_ptr  (set up by game in Function_92116C60)
          //   +0x18: ready_flag  (wait loop polls this, needs non-zero)
          //   +0x38: secondary_flag
          //
          // The wait loop (Function_921131A0) takes this object as 'this':
          //   while (this->ready_flag == 0) {
          //     vtable[3](this); sleep(1000);
          //   }
          //
          // Previously we wrote 0 to +0x00, DESTROYING the vtable.
          // Now: DO NOT touch +0x00. Write device handle to +0x18 to unblock.

          if (status_ptr) {
            uint32_t ready_flag_addr = status_ptr + 0x18;

            // First, read the current vtable at +0x00 to verify it's set:
            auto* vtable_slot = memory_->TranslateVirtual<uint32_t*>(status_ptr);
            if (vtable_slot) {
              uint32_t vtable_val = xe::load_and_swap<uint32_t>(vtable_slot);
              XELOGI("XamApp: 0x2B001 NUI object +0x00 vtable={:08X}", vtable_val);
            }

            // Write device handle to +0x18 (ready flag) to unblock wait loop:
            auto* ready_slot = memory_->TranslateVirtual<uint32_t*>(ready_flag_addr);
            if (ready_slot) {
              xe::memory::PageAccess rp_old;
              xe::memory::Protect(ready_slot, 4, xe::memory::PageAccess::kReadWrite, &rp_old);
              xe::store_and_swap<uint32_t>(ready_slot, cached_device_addr);
              xe::memory::Protect(ready_slot, 4, rp_old);
              XELOGI("XamApp: 0x2B001 wrote device {:08X} to NUI ready_flag {:08X}",
                     cached_device_addr, ready_flag_addr);
            }

            // Verify readback:
            if (ready_slot) {
              uint32_t readback = xe::load_and_swap<uint32_t>(ready_slot);
              XELOGI("XamApp: 0x2B001 VERIFY ready_flag={:08X} readback={:08X}",
                     ready_flag_addr, readback);
            }
          }

          XELOGI("XamApp: 0x2B001 done: event={:08X} nui_obj={:08X} device={:08X}",
                 original_handle, status_ptr, cached_device_addr);
        }
      }
      return X_E_SUCCESS;
    }
    case 0x0002B002: {
      // NUI skeleton tracking enable / configure.
      // The game sends this after 0x2B001/0x2B004 to start skeleton tracking.
      auto* out = memory_->TranslateVirtual<uint32_t*>(buffer_ptr);
      if (out && buffer_length >= 4) {
        XELOGI("XamApp: 0x2B002 raw buffer: {:08X} {:08X} {:08X} {:08X}",
               xe::byte_swap(out[0]), xe::byte_swap(out[1]),
               xe::byte_swap(out[2]), xe::byte_swap(out[3]));
      }
      // Ensure KinectInputDriver is initialized for skeleton tracking.
      if (kd() && !kd()->is_initialized()) {
        kd()->NuiInitialize(0x08);
      }
      XELOGI("XamApp: 0x2B002 skeleton tracking enable → S_OK");
      return X_E_SUCCESS;
    }
    case 0x0002B003: {
      // Games used in:
      // 4D5309C9
      // It only receives buffer
      struct {
        xe::be<uint64_t> unk1;
        xe::be<uint64_t> unk2;
        xe::be<uint64_t> unk3;
      }* args = memory_->TranslateVirtual<decltype(args)>(buffer_ptr);
 
      XELOGI("XamUnk2B003({:016X}, {:016X}, {:016X}), unimplemented",
             args->unk1.get(), args->unk2.get(), args->unk3.get());
      return X_E_SUCCESS;
    }
    case 0x0002B004: {
      // NUI subsystem startup -- attempt device open.
      auto* driver = kd();
      if (!driver) {
        XELOGW("XamApp: 0x2B004 no KinectInputDriver");
        return X_E_FAIL;
      }
      if (!driver->is_initialized()) {
        // NUI_INITIALIZE_FLAG_USES_SKELETON
        X_RESULT result = driver->NuiInitialize(0x08);
        if (result != X_ERROR_SUCCESS) {
          XELOGE("XamApp: 0x2B004 NuiInitialize failed ({:08X})", result);
          return X_E_FAIL;
        }
      }
      XELOGI("XamApp: 0x2B004 NUI device ready");
      return X_E_SUCCESS;
    }
    case 0x0002B005: {
      return X_E_SUCCESS;
    }
    case 0x00021028: {
      return X_E_SUCCESS;
    }
    case 0x00021030: {
      return X_E_SUCCESS;
    }
    // Causes dashboard to correctly process language/region change. It does not
    // contain any buffer.
    case 0x8000000D: {
      const bool is_pc_enabled =
          (kernel_state_->xconfig()->ReadSetting<uint8_t>(
               XCONFIG_USER_CATEGORY, XCONFIG_USER_PC_FLAGS) &
           X_PC_FLAGS::PCEnabled) != 0;

      return is_pc_enabled ? X_E_ACCESS_DENIED : X_E_SUCCESS;
    }
  }
  XELOGE(
      "Unimplemented XAM message app={:08X}, msg={:08X}, arg1={:08X}, "
      "arg2={:08X}",
      app_id(), message, buffer_ptr, buffer_length);
  return X_E_FAIL;
}

}  // namespace apps
}  // namespace xam
}  // namespace kernel
}  // namespace xe
