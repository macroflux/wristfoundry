#ifndef WRISTFOUNDRY_USB_BRIDGE_H_
#define WRISTFOUNDRY_USB_BRIDGE_H_
#include <windows.h>
#include <flutter/binary_messenger.h>
#include <flutter/encodable_value.h>
#include <flutter/method_channel.h>
#include <memory>

class UsbBridge {
 public:
  explicit UsbBridge(flutter::BinaryMessenger* messenger);
  ~UsbBridge();
 private:
  void Handle(const flutter::MethodCall<flutter::EncodableValue>& call,
              std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void Close();
  HANDLE port_ = INVALID_HANDLE_VALUE;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
};
#endif
