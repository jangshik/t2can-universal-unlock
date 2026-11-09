"""Exercise the production visibility API/scheduler with fake IDF/NVS adapters."""
from pathlib import Path
import subprocess, tempfile, re
ROOT=Path(__file__).resolve().parents[1]
web=(ROOT/'web_api.h').read_text()
def function(name):
    m=re.search(r'^static[^;{}]*\b'+name+r'\([^;{}]*\)\s*\{',web,re.M);assert m,name
    end=web.index('{',m.start())+1;depth=1
    while depth:
        depth+=(web[end]=='{')-(web[end]=='}');end+=1
    return web[m.start():end]+'\n'
code=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <cstdio>
struct String:std::string {using std::string::string;String(const std::string&s):std::string(s){} void concat(const char*s,size_t n){append(s,n);}
 using std::string::operator+=;String& operator+=(unsigned long n){append(std::to_string(n));return *this;}String& operator+=(long n){append(std::to_string(n));return *this;}
};
#include "json_writer_arduino.h"
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,WIFI_IF_AP=1,WIFI_STORAGE_RAM=1;
struct wifi_config_t {struct {uint8_t ssid[32],password[64],ssid_hidden,channel,max_connection;uint16_t beacon_interval;}ap;} driver;
uint64_t now=0;bool found=false,readOk=true,writeOk=true;
uint8_t saved[8]={};int nvsWrites=0,getCalls=0,setCalls=0,storageCalls=0,getError=0,setError=0,storageError=0;
int64_t esp_timer_get_time(){return now*1000;}
const char* esp_err_to_name(esp_err_t e){return e==0?"ESP_OK":"ESP_FAIL";}
bool nvsCheckedReadBlob(const char* ns,const char* key,void* b,size_t n,bool& present){assert(!strcmp(ns,"wifiap")&&!strcmp(key,"visibility")&&n==8);present=found;if(found)memcpy(b,saved,n);return readOk;}
bool nvsCheckedWriteBlob(const char* ns,const char* key,const void* b,size_t n){assert(!strcmp(ns,"wifiap")&&!strcmp(key,"visibility")&&n==8);++nvsWrites;if(!writeOk)return false;memcpy(saved,b,n);found=true;return true;}
esp_err_t esp_wifi_get_config(int iface,wifi_config_t* c){assert(iface==WIFI_IF_AP);++getCalls;if(getError)return getError;*c=driver;return ESP_OK;}
esp_err_t esp_wifi_set_storage(int storage){assert(storage==WIFI_STORAGE_RAM);++storageCalls;return storageError;}
esp_err_t esp_wifi_set_config(int iface,wifi_config_t* c){assert(iface==WIFI_IF_AP&&storageCalls>0);++setCalls;if(setError)return setError;driver=*c;return ESP_OK;}
#include "wifi_ap_visibility_runtime.h"
struct Server {
 std::vector<std::pair<String,String>> query;int status=0;String body;
 int args(){return query.size();}String argName(int n){return query[n].first;}String arg(int n){return query[n].second;}
 void send(int s,const char*,String b){status=s;body=b;}void sendHeader(const char*,const char*){}
}server;
'''
code+=function('wifiApWriteVisibility')+function('httpWifiVisibility')
code+=r'''
int main(){
 memset(&driver,0x5a,sizeof(driver));driver.ap.ssid_hidden=0;
 const auto original=driver;
 wifiApVisibilityLoad();wifiApVisibilityApStarted(false);wifiApVisibilityService();
 assert(wifiApVisibilityConfig.mode==0&&nvsWrites==0&&setCalls==0);
 auto post=[](std::vector<std::pair<String,String>> q){server.query=q;httpWifiVisibility();return server.status;};
 now=2000;assert(post({{"mode","2"},{"hideAfterSec","10"}})==200);
 assert(nvsWrites==1&&setCalls==0&&server.body.find("\"hideInSec\":8")!=String::npos);
 now=9999;wifiApVisibilityService();assert(setCalls==0);
 now=10000;wifiApVisibilityService();assert(setCalls==1&&driver.ap.ssid_hidden==1&&nvsWrites==1);
 auto expected=original;expected.ap.ssid_hidden=1;assert(!memcmp(&expected,&driver,sizeof(driver)));
 wifiApVisibilityService();assert(setCalls==1); // no repeated writes every web tick
 now=uint64_t(UINT32_MAX)+10001;wifiApVisibilityService();assert(driver.ap.ssid_hidden==1&&setCalls==1);
 assert(post({{"mode","0"},{"hideAfterSec","10"}})==200);
 wifiApVisibilityService();assert(driver.ap.ssid_hidden==1);
 now+=599;wifiApVisibilityService();assert(driver.ap.ssid_hidden==1);
 ++now;wifiApVisibilityService();assert(!driver.ap.ssid_hidden&&!memcmp(&original,&driver,sizeof(driver)));
 assert(nvsWrites==2); // runtime visibility does not write flash
 const int writes=nvsWrites;
 for(auto args:std::vector<std::vector<std::pair<String,String>>>{
   {{"mode","3"},{"hideAfterSec","10"}},{{"mode","1"},{"hideAfterSec","3601"}},
   {{"mode","1"},{"hideAfterSec","-1"}},{{"mode","1"},{"hideAfterSec",""}},
   {{"mode","1"},{"hideAfterSec","10x"}},{{"mode","1"},{"hideAfterSec","65536"}},
   {{"mode","1"},{"mode","2"}},{{"mode","1"}},{{"mode","1"},{"unknown","0"}}}){
   assert(post(args)==400&&nvsWrites==writes&&wifiApVisibilityConfig.mode==0);
 }
 writeOk=false;assert(post({{"mode","1"},{"hideAfterSec","10"}})==503&&wifiApVisibilityConfig.mode==0);
 now+=600;wifiApVisibilityService();assert(!driver.ap.ssid_hidden);writeOk=true;
 assert(post({{"mode","2"},{"hideAfterSec","10"}})==200);now+=600;wifiApVisibilityService();assert(driver.ap.ssid_hidden); // boot deadline already passed
 now=1000;wifiApVisibilityLoad();assert(wifiApVisibilityConfig.mode==2&&wifiApVisibilityConfig.hideAfterSec==10);
 assert(!wifiApVisibilityTarget(now));driver.ap.ssid_hidden=0;wifiApVisibilityApStarted(false);
 now=10000;wifiApVisibilityService();assert(driver.ap.ssid_hidden);
 // Zero delay, always hidden and corrupt-record fallback.
 assert(post({{"mode","2"},{"hideAfterSec","0"}})==200);now=0;wifiApVisibilityLoad();assert(wifiApVisibilityTarget(now));
 assert(post({{"mode","1"},{"hideAfterSec","3600"}})==200);now=0;wifiApVisibilityLoad();assert(wifiApVisibilityTarget(now));
 saved[4]=1;wifiApVisibilityLoad();assert(wifiApVisibilityNvsError&&wifiApVisibilityConfig.mode==0);saved[4]=0;
 readOk=false;wifiApVisibilityLoad();assert(wifiApVisibilityNvsError&&wifiApVisibilityConfig.mode==0);readOk=true;
 // Driver failures get three attempts at 5-second intervals, no restart fallback.
 now=1000;driver.ap.ssid_hidden=0;wifiApVisibilityApStarted(false);
 assert(post({{"mode","1"},{"hideAfterSec","60"}})==200);setError=ESP_FAIL;
 const int before=setCalls;now+=600;wifiApVisibilityService();assert(setCalls==before+1&&wifiApVisibilityError==ESP_FAIL);
 for(int i=0;i<100;++i){wifiApVisibilityService();}
 assert(setCalls==before+1);
 now+=5000;wifiApVisibilityService();now+=5000;wifiApVisibilityService();now+=5000;wifiApVisibilityService();
 assert(setCalls==before+3&&!driver.ap.ssid_hidden);
 const int savedWrites=nvsWrites;setError=0;
 assert(post({{"mode","1"},{"hideAfterSec","60"}})==200&&nvsWrites==savedWrites);
 now+=600;wifiApVisibilityService();assert(driver.ap.ssid_hidden&&wifiApVisibilityError==ESP_OK);
 // AP fallback stays discoverable; a new explicit save can re-enable hiding.
 wifiApVisibilityFallback=true;driver.ap.ssid_hidden=0;wifiApVisibilityApStarted(false);
 now+=100000;wifiApVisibilityService();assert(!driver.ap.ssid_hidden&&!wifiApVisibilityTarget(now));
 assert(post({{"mode","1"},{"hideAfterSec","60"}})==200);now+=600;wifiApVisibilityService();assert(driver.ap.ssid_hidden);
 // Failed read/storage must not claim applied state or call set_config.
 assert(post({{"mode","0"},{"hideAfterSec","60"}})==200);now+=600;getError=ESP_FAIL;
 const auto setBefore=setCalls;wifiApVisibilityService();assert(!wifiApVisibilityKnown&&setCalls==setBefore);
 getError=0;storageError=ESP_FAIL;now+=5000;wifiApVisibilityService();assert(setCalls==setBefore&&wifiApSsidHidden);
 storageError=0;now+=5000;wifiApVisibilityService();assert(setCalls==setBefore+1&&!wifiApSsidHidden);
 puts("Wi-Fi visibility: API, persistence, boot deadline, wrap, live config preservation, bounded retries PASS");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';cpp.write_text(code)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
# The dedicated live path has no lifecycle, controller or scheduler side effects.
runtime=(ROOT/'wifi_ap_visibility_runtime.h').read_text()
for token in ('esp_restart','ESP.restart','softAPdisconnect','wifiApActivate','WiFi.mode','esp_wifi_stop','esp_wifi_start','xTaskCreate','delay(','vTaskDelay','canTxBarrier','portENTER_CRITICAL'):
    assert token not in runtime,token
assert 'WiFi.persistent(false);' in web
assert 'server.handleClient();\n    wifiApVisibilityService();' in web
print('Visibility path has no MCU/AP restart, task/core changes, CAN locks or blocking delay PASS')
