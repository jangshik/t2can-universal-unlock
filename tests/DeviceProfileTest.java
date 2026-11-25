package dev.t2can.unlock;
public final class DeviceProfileTest {
    static int checks;
    static void check(boolean ok, String name) { checks++; if (!ok) throw new AssertionError(name); }
    static DeviceProfile p(String ssid,String security,String password) {
        return new DeviceProfile("1","Device",ssid,security,password,UrlPolicy.HOME);
    }
    public static void main(String[] args) {
        check(DeviceProfile.legacy().validationError(false)==null,"legacy manual supported");
        check(DeviceProfile.legacy().validationError(true)!=null,"automatic requires actual SSID");
        check(p("T2CAN-1234","WPA2","12345678").validationError(true)==null,"WPA2 accepted");
        check(p("T2CAN-1234","WPA3","12345678").validationError(true)==null,"WPA3 accepted");
        check(p("AP","OPEN","").validationError(true)==null,"open accepted");
        check(p("AP","OPEN","secret").validationError(true)!=null,"open credentials rejected");
        check(p("AP","WEP","12345678").validationError(true)!=null,"unsupported security rejected");
        check(p("AP","WPA2","short").validationError(true)!=null,"short WPA2 rejected");
        check(p("AP","WPA2",String.join("",java.util.Collections.nCopies(64,"a"))).validationError(true)!=null,"64 char pass rejected");
        check(p("AP","WPA2","1234567é").validationError(true)!=null,"non ASCII passphrase rejected");
        check(p(String.join("",java.util.Collections.nCopies(11,"한")),"OPEN","").validationError(true)!=null,"UTF8 SSID bytes capped");
        check(p("AP\n","OPEN","").validationError(true)!=null,"control SSID rejected");
        check(DeviceProfile.legacy().type.equals(DeviceProfile.TYPE_T2CAN),"legacy is T2CAN");
        check(!p("AP","OPEN","").generic(),"six-arg profile defaults to T2CAN");
        check(p("AP","OPEN","").healthPath().equals("/api/profile/status"),"T2CAN probes profile API");
        DeviceProfile g=new DeviceProfile("2","ESP32","ESP32-AP","OPEN","",UrlPolicy.HOME,DeviceProfile.TYPE_GENERIC);
        check(g.validationError(true)==null && g.generic(),"generic accepted");
        check(g.healthPath().equals("/"),"generic probes root");
        check(new DeviceProfile("3","X","AP","OPEN","",UrlPolicy.HOME,"OTHER").validationError(false)!=null,"unknown type rejected");
        check(new DeviceProfile("3","X","AP","OPEN","",UrlPolicy.HOME,null).validationError(false)!=null,"null type rejected");
        check(DeviceProfile.genericStatusOk(200) && DeviceProfile.genericStatusOk(302),"2xx/3xx accepted");
        check(!DeviceProfile.genericStatusOk(404) && !DeviceProfile.genericStatusOk(500) && !DeviceProfile.genericStatusOk(-1),"errors rejected");
        System.out.println("DeviceProfileTest: "+checks+" checks PASS");
    }
}
