# 1.2.2 changes from 1.2.1

- DeviceProfile: new device type, `T2CAN` (default) or `GENERIC`. Profiles saved by 1.2.1 and earlier have no type and load as `T2CAN`; the encrypted storage format version is unchanged.
- ConnectionManager: generic profiles are verified with `GET /` on the profile origin. Any HTTP 2xx/3xx response passes (redirects are not followed, the body is not read). T2CAN profiles keep the `/api/profile/status` check. Diagnostics include `profileType`.
- MainActivity: generic dashboards skip the T2CAN bridge, native message port and theme hook. Back navigates WebView history, and the Connection menu opens without the transfer-busy query. HTTP errors on later navigations no longer disconnect a generic device.
- ConnectionPanel: device type selector in Add/Edit device; manual-connection text no longer names T2CAN.
- URL policy, cleartext rules and same-origin request blocking are unchanged for both types (HTTP only at 192.168.4.1; external CDN resources stay blocked).
- App name is now "Tesla Unlock Universal"; APK/source ZIP are named `Tesla-Unlock-Universal-1.2.2*`.
- Application ID is now `dev.t2can.unlock.universal` (set by `aapt2 --rename-manifest-package` in tools/build.py; Java package and R namespace stay `dev.t2can.unlock`). Signed with a new release key (alias `tesla-unlock`). It installs alongside the original app rather than updating it; profiles saved in the original app are not carried over.
- Emulator tests target the new application ID; the launch test registers its profile in 1.2.2 instead of updating from 1.1.0.
- Version 1.2.2 / code 6.

# 1.2.1 changes from 1.2.0

- ConnectionManager: onUnavailable performs one immediate fresh automatic Wi-Fi request after teardown. No retiring-network probes on retry. Other errors do not retry.
- ConnectionRetryPolicy: one retry budget per launch/manual Connect sequence; disconnect and terminal failure cancel it.
- Waiting message now says Android Wi-Fi connection, rather than implying a user approval dialog is necessarily visible.
- Version 1.2.1 / code 5; signing identity unchanged.
- Added retry policy regression tests; updated launch emulator APK target.
- Original release directories remain unchanged.
