# WristFoundry companion mark

The WF concept uses bold angular letterforms, pale ice lettering and cyan
accents on near-black navy. A faint diagonal dot lattice is strongest behind
the monogram and fades toward the square's edges.

- Master/app asset: `desktop/assets/wristfoundry-mark.png`.
- Android legacy launcher densities and adaptive layers: `android-native/res`.
  `scripts/prepare_android.py` overlays these onto the generated Android project.
  The adaptive foreground has extra padding so common launcher masks preserve WF.
- Windows multi-resolution icon: `desktop/windows/runner/resources/app_icon.ico`.
- Both Flutter headers load the shared app asset.

This is the initial generated branding concept, not a vector master. The checked-in
launcher assets are already sized for packaging; build machines do not need an
image editor or additional Python image dependencies.
