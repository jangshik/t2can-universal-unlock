#include "../feature_actions.h"
#include <cassert>
#include <set>
#include <string>
int main(){
  std::set<unsigned> ids;std::set<std::string> codes;
  for(const auto &info:featureActions){
    assert(info.id>=14);assert(ids.insert(info.id).second);assert(codes.insert(info.code).second);
    uint8_t parsed=255;assert(featureActionParse(info.code,parsed)&&parsed==info.id);
    assert(featureActionParse(info.label,parsed)&&parsed==info.id);
    assert(!strcmp(featureActionCode(parsed),info.code));
  }
  uint8_t parsed=99;assert(!featureActionParse("bogus",parsed)&&parsed==99);assert(!featureActionParse(nullptr,parsed));assert(!featureActionInfo(255));
  FeatureActionQueue q;uint8_t action=0;
  assert(!q.pop(0,action));assert(!q.push(0,0));assert(!q.push(255,0));
  for(unsigned i=0;i<FeatureActionQueue::capacity;i++)assert(q.push(uint8_t(i+1),0));
  assert(!q.push(14,0));
  for(unsigned i=0;i<FeatureActionQueue::capacity;i++){assert(q.pop(1000,action));assert(action==i+1);}
  assert(!q.pop(1000,action));
  assert(q.push(14,0));assert(q.push(15,1000));assert(q.pop(1001,action)&&action==15);
  assert(q.push(14,UINT32_MAX-100));assert(q.pop(100,action)&&action==14);
  assert(q.push(14,UINT32_MAX-100));assert(!q.pop(1000,action));
  assert(q.push(14,100));q.clear();assert(!q.pop(101,action));
}
