# WristFoundry Companion

One Flutter app supports Windows over USB and Android over authenticated BLE.
Shared controllers, settings and protocol code are in `lib`; the Android native
bridge lives in `../android-native` and Windows code lives in `windows`.

See [Windows setup](../../docs/COMPANION.md) and
[Android setup and acceptance](../../docs/ANDROID_COMPANION.md).

Use Flutter 3.38.7. Run `python scripts/prepare_android.py` from the repository
root before an Android build. Install the complete Windows runtime directory;
do not move its EXE away from its DLLs/data. Android debug APKs are for testing.
