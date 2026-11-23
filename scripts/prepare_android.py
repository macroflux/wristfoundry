"""Generate Flutter 3.38.7 Android scaffolding, then install the native BLE bridge.

Generated Gradle/wrapper files stay build output. The native bridge and permission
declarations below are the reviewed Android source inputs.
"""
import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = ROOT / "companion/desktop"
generated_extras = [APP / "test/widget_test.dart", APP / "analysis_options.yaml"]
existing_extras = {p for p in generated_extras if p.exists()}
subprocess.run([
    "flutter", "create", "--platforms=android", "--org=io.wristfoundry",
    "--project-name=wristfoundry_companion", "--no-pub", str(APP),
], check=True, shell=__import__('os').name == 'nt')
for path in generated_extras:
    if path not in existing_extras and path.exists():
        path.unlink()  # Counter-template tests/lints do not belong to this app.
target = APP / "android/app/src/main/kotlin/io/wristfoundry/wristfoundry_companion/MainActivity.kt"
target.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(ROOT / "companion/android-native/MainActivity.kt", target)
manifest = APP / "android/app/src/main/AndroidManifest.xml"
content = manifest.read_text()
permissions = '''
    <uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
    <uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
'''
if 'android.permission.BLUETOOTH_SCAN' not in content:
    content = content.replace('    <application', permissions + '    <application', 1)
content = content.replace('android:label="wristfoundry_companion"', 'android:label="WristFoundry Companion"')
manifest.write_text(content)
gradle = APP / "android/app/build.gradle.kts"
content = gradle.read_text().replace('minSdk = flutter.minSdkVersion', 'minSdk = 23')
gradle.write_text(content)
print('Android bridge prepared; build an APK with flutter build apk --debug.')
