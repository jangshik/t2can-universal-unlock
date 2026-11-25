package dev.t2can.unlock;

import java.nio.charset.StandardCharsets;

/** User-supplied AP details. No credential is derived from a device name. */
public final class DeviceProfile {
    /** T2CAN firmware: verified through /api/profile/status and the dashboard bridge. */
    public static final String TYPE_T2CAN = "T2CAN";
    /** Any other device web UI (e.g. a plain ESP32 SoftAP): verified by an HTTP response at "/". */
    public static final String TYPE_GENERIC = "GENERIC";
    public final String id, name, ssid, security, password, url, type;
    public DeviceProfile(String id, String name, String ssid, String security, String password, String url) {
        this(id, name, ssid, security, password, url, TYPE_T2CAN);
    }
    public DeviceProfile(String id, String name, String ssid, String security, String password, String url, String type) {
        this.id = id; this.name = name; this.ssid = ssid; this.security = security;
        this.password = password; this.url = url; this.type = type;
    }
    public boolean generic() { return TYPE_GENERIC.equals(type); }
    /** Path probed before the dashboard opens. */
    public String healthPath() { return generic() ? "/" : "/api/profile/status"; }
    /** Generic devices only need to answer like a web server; redirects are not followed. */
    public static boolean genericStatusOk(int code) { return code >= 200 && code < 400; }
    public static DeviceProfile legacy() {
        return new DeviceProfile("legacy", "T2CAN", "", "OPEN", "", UrlPolicy.HOME);
    }
    public String validationError(boolean automatic) {
        if (id == null || id.isEmpty() || id.length() > 128) return "Invalid profile ID.";
        if (name == null || name.trim().isEmpty() || name.length() > 80 || controls(name))
            return "Enter a device name (1–80 characters).";
        if (ssid == null || controls(ssid) || ssid.getBytes(StandardCharsets.UTF_8).length > 32
                || (automatic && ssid.isEmpty())) return "Enter the exact Wi-Fi SSID (1–32 UTF-8 bytes).";
        if (!TYPE_T2CAN.equals(type) && !TYPE_GENERIC.equals(type)) return "Choose a device type.";
        if (!"OPEN".equals(security) && !"WPA2".equals(security) && !"WPA3".equals(security))
            return "Choose Open, WPA2 or WPA3 security.";
        if (password == null) return "Enter the Wi-Fi password.";
        if ("OPEN".equals(security)) {
            if (!password.isEmpty()) return "Open networks must not have a password.";
        } else {
            if (password.length() < 8 || password.length() > 63)
                return "Wi-Fi passwords must contain 8–63 ASCII characters.";
            for (int i = 0; i < password.length(); i++)
                if (password.charAt(i) < 32 || password.charAt(i) > 126)
                    return "Wi-Fi passwords must contain 8–63 ASCII characters.";
        }
        if (!UrlPolicy.validHome(url)) return "Use HTTP at 192.168.4.1 or HTTPS at a private IPv4 address, with no path, query or fragment.";
        return null;
    }
    private static boolean controls(String value) {
        for (int i = 0; i < value.length(); i++) if (Character.isISOControl(value.charAt(i))) return true;
        return false;
    }
}
