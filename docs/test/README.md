# 测试

`feature_list_debug.cc` 中保存需要改写的文件名:

```cpp
// feature_list_debug.cc

#include <stdint.h>
#include <unordered_map>
#include <vector>

#include "teams/ad/ad_algorithm/feature/fast/impl/extract_ad_delay_label.h"

namespace ks {
namespace ad_algorithm {

}  // namespace
}  // namespace
```

执行如下命令可以进行改写

```bash
convert feature_list_debug.cc --cmd=convert --field-detail-filename=field.json --use_reco_user_info=false --overwrite --dump-ast -- pthread  -MMD -march=haswell -march=haswell -Wno-deprecated-builtins -I/usr/local/include/c++/v1 -march=haswell -Iinfra/ -Ipub/src/infra/component_usage_tracker/src/ -Ithird_party/apache-arrow/arrow-8.0.1/cpp/src -fPIC -Wno-inconsistent-missing-override -Werror=return-type -Wtrigraphs -Wuninitialized -Wimplicit-const-int-float-conversion -Wwrite-strings -Wpointer-arith -Wmissing-include-dirs -Wno-unused-function -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-implicit-fallthrough  -Wno-deprecated-declarations -Wno-missing-field-initializers -Wno-missing-include-dirs -std=c++17 -Wvla -Wnon-virtual-dtor -Woverloaded-virtual  -Wno-invalid-offsetof -Werror=non-virtual-dtor -O3 -Wformat=2 -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -Wframe-larger-than=262143 -ggdb3 -Wno-format-nonliteral  -Wno-register -DENABLE_KUIBA -DASIO_STANDALONE -DBRPC_WITH_GLOG=1 -DBTHREAD_USE_FAST_PTHREAD_MUTEX -DGFLAGS_NS=google -DHAVE_PTHREAD -DHAVE_ZLIB=1 -DNO_DUMMY_DECL -DOSATOMIC_USE_INLINED=1 -DPB_FIELD_32BIT -DTHREADED -D_ONLY_GET_SYNC_PAIRS_CONF -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D__const__=  -Wno-implicit-fallthrough -Wno-non-virtual-dtor -Wno-vla -D__STDC_FORMAT_MACROS -DUSE_SYMBOLIZE -DPIC -I.build/pb/c++ -Iprebuilt/include -I./third_party -I. -DNDEBUG -DUSE_TCMALLOC=1 -DENABLE_TCMALLOC=1 -nostdinc++ -nodefaultlibs -Werror -I/usr/java/default/include/ -I/usr/java/default/include/linux -MT .build/opt/objs/teams/ad/ad_algorithm/bs_feature/bs_fea_util/fast/frame/bs_leaf_util.o -o .build/opt/objs/teams/ad/ad_algorithm/bs_feature/bs_fea_util/fast/frame/bs_leaf_util.o
```
