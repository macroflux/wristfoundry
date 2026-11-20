#include "usb_bridge.h"
#include <setupapi.h>
#include <devguid.h>
#include <flutter/standard_method_codec.h>
#include <algorithm>
#include <string>
#include <vector>

namespace {
using Value = flutter::EncodableValue;
using Map = flutter::EncodableMap;

std::string Utf8(const wchar_t* value) {
  int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (count <= 1) return {};
  std::string out(static_cast<size_t>(count), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), count, nullptr, nullptr);
  out.resize(static_cast<size_t>(count - 1));
  return out;
}

std::wstring Property(HDEVINFO devices, SP_DEVINFO_DATA* device, DWORD key) {
  wchar_t value[1024] = {};
  if (!SetupDiGetDeviceRegistryPropertyW(devices, device, key, nullptr,
      reinterpret_cast<PBYTE>(value), sizeof(value), nullptr)) return {};
  value[1023] = 0;
  return value;
}

flutter::EncodableList Enumerate() {
  flutter::EncodableList result;
  HDEVINFO devices = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
  if (devices == INVALID_HANDLE_VALUE) return result;
  SP_DEVINFO_DATA device = {};
  device.cbSize = sizeof(device);
  for (DWORD i = 0; i < 128 && SetupDiEnumDeviceInfo(devices, i, &device); ++i) {
    HKEY key = SetupDiOpenDevRegKey(devices, &device, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE) continue;
    wchar_t port[64] = {};
    DWORD type = 0, bytes = sizeof(port);
    LSTATUS status = RegQueryValueExW(key, L"PortName", nullptr, &type, reinterpret_cast<LPBYTE>(port), &bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_SZ) continue;
    port[63] = 0;
    std::wstring port_name(port);
    if (port_name.size() < 4 || port_name.substr(0,3) != L"COM" ||
        port_name.find_first_not_of(L"0123456789", 3) != std::wstring::npos) continue;
    auto friendly = Property(devices, &device, SPDRP_FRIENDLYNAME);
    auto hardware = Property(devices, &device, SPDRP_HARDWAREID);
    wchar_t instance[1024] = {};
    SetupDiGetDeviceInstanceIdW(devices, &device, instance, 1024, nullptr);
    // Only native Espressif USB JTAG/serial is prioritized. This is a candidate
    // hint, never proof of product identity. Enumeration does not open ports.
    bool candidate = hardware.find(L"VID_303A&PID_1001") != std::wstring::npos;
    result.emplace_back(Map{
      {Value("port"), Value(Utf8(port))},
      {Value("label"), Value(Utf8(friendly.empty() ? port : friendly.c_str()))},
      {Value("usb_id"), Value(Utf8(instance))},
      {Value("candidate"), Value(candidate)},
    });
  }
  SetupDiDestroyDeviceInfoList(devices);
  return result;
}
}

UsbBridge::UsbBridge(flutter::BinaryMessenger* messenger) {
  channel_ = std::make_unique<flutter::MethodChannel<Value>>(
      messenger, "io.wristfoundry/usb", &flutter::StandardMethodCodec::GetInstance());
  channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    Handle(call, std::move(result));
  });
}
UsbBridge::~UsbBridge() { channel_->SetMethodCallHandler(nullptr); Close(); }
void UsbBridge::Close() {
  if (port_ != INVALID_HANDLE_VALUE) { CloseHandle(port_); port_ = INVALID_HANDLE_VALUE; }
}

void UsbBridge::Handle(const flutter::MethodCall<Value>& call,
                      std::unique_ptr<flutter::MethodResult<Value>> result) {
  const auto& method = call.method_name();
  if (method == "enumerate") { result->Success(Value(Enumerate())); return; }
  if (method == "close") { Close(); result->Success(); return; }
  if (method == "open") {
    if (port_ != INVALID_HANDLE_VALUE) { result->Error("port_in_use"); return; }
    const auto* args = std::get_if<Map>(call.arguments());
    if (!args) { result->Error("invalid_argument"); return; }
    auto value = args->find(Value("port"));
    const auto* name = value == args->end() ? nullptr : std::get_if<std::string>(&value->second);
    if (!name || name->size() < 4 || name->size() > 12 || name->substr(0,3) != "COM" ||
        name->find_first_not_of("0123456789", 3) != std::string::npos) {
      result->Error("invalid_argument"); return;
    }
    std::string path = "\\\\.\\" + *name;
    port_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (port_ == INVALID_HANDLE_VALUE) {
      DWORD error = GetLastError();
      result->Error(error == ERROR_SHARING_VIOLATION ? "port_in_use" :
          error == ERROR_ACCESS_DENIED ? "access_denied" : "open_failed"); return;
    }
    DCB config = {};
    config.DCBlength = sizeof(config);
    if (!GetCommState(port_, &config)) { Close(); result->Error("open_failed"); return; }
    config.BaudRate = CBR_115200; config.ByteSize = 8; config.Parity = NOPARITY;
    config.StopBits = ONESTOPBIT; config.fBinary = TRUE; config.fParity = FALSE;
    config.fDtrControl = DTR_CONTROL_DISABLE; config.fRtsControl = RTS_CONTROL_DISABLE;
    config.fOutxCtsFlow = FALSE; config.fOutxDsrFlow = FALSE;
    config.fDsrSensitivity = FALSE; config.fOutX = FALSE; config.fInX = FALSE;
    config.fAbortOnError = FALSE;
    COMMTIMEOUTS timeouts = {};
    // Reads always return immediately; the Dart poll never waits for a byte.
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.WriteTotalTimeoutConstant = 100;
    if (!SetCommState(port_, &config) || !SetCommTimeouts(port_, &timeouts)) {
      Close(); result->Error("open_failed"); return;
    }
    // Do not call EscapeCommFunction, send a break, toggle DTR/RTS, or invoke
    // Espressif's bootloader sequence. Verify OS/driver line behavior physically.
    result->Success(); return;
  }
  if (port_ == INVALID_HANDLE_VALUE) { result->Error("disconnected"); return; }
  if (method == "read") {
    DWORD errors = 0;
    COMSTAT stat = {};
    if (!ClearCommError(port_, &errors, &stat)) { Close(); result->Error("disconnected"); return; }
    DWORD count = (std::min)(stat.cbInQue, DWORD{8192});
    std::vector<uint8_t> buffer(count);
    DWORD received = 0;
    if (count && !ReadFile(port_, buffer.data(), count, &received, nullptr)) {
      Close(); result->Error("disconnected"); return;
    }
    buffer.resize(received);
    result->Success(Value(buffer)); return;
  }
  if (method == "write") {
    const auto* data = std::get_if<std::vector<uint8_t>>(call.arguments());
    if (!data || data->size() > 1025) { result->Error("invalid_argument"); return; }
    DWORD written = 0;
    bool ok = WriteFile(port_, data->data(), static_cast<DWORD>(data->size()), &written, nullptr) != FALSE;
    if (!ok || written != data->size()) { Close(); result->Error("write_failed"); return; }
    result->Success(); return;
  }
  result->NotImplemented();
}
