# 从 adlog 特征类自动转 BS 特征类

## 转换逻辑

转换的时候主要需要考虑以下逻辑怎么转换

1. if
2. for
3. common info attr
4. 中间节点调用，如 PhotoInfo
5. 参数中有 proto 字段的自定义函数

转换时候只需要考虑 bs 字段相关的代码，转换时候有些信息是从子节点拿到的，因此需要有 env 来保存相关的信息，
只有 if 和 for 有 env。

## 表达式

每个 adlog 字段的相关调用都是一个表达式，经过解析后包含转换所需要的信息，比较重要的信息包括 bs_expr、
env 、common info attr 枚举、for 循环变量等。

## 实现

最终转换的时候，需要根据表达式类型进行处理，对应以下逻辑:

1. CXXMemberCallExpr: 如果是以 adlog 开头的表达式，则替换成 bs 的表达式，会有一些比较复杂的情况，如
   common_info 的 list、 map, action_detail 的 list ，photo info 等中间节点对应的字段。
2. IfStmt: 需要区分是否是 common info ，中间定义的临时变量需要删除。
3. for 循环: 需要记录循环变量，区分是否是 common info。对于 map、list 等类型，需要在循环外面定义新变量。
4. operator[]: 如果是 adlog 字段，则替换成 key_xxx 对应的 bs 的值。

转换时候的全部信息都可以分为两类，一类是环境信息，一类是 expr 包含的信息，分别用 Env 和 ExprInfo 代替。转换时候
首先从 ExprInfo 获取信息，如果不够则从 Env 获取。需要的信息必须要在转换之前添加到 Env 或者 ExprInfo 中, 通过
env.update 或者 parse_expr 实现。

Env 保存了当前 scope 中能访问到的各种变量，以及额外的信息，如 common info enum、action_detail no 等, 只有
三种情况会创建新的 Env:

1. 每个提特征类开始时候需要创建 Env。
2. 每个 IfStmt 会创建 Env。
3. 每个 for 循环会创建 Env。

访问 Stmt 时候会先添加一些比较简单的变量信息到 Env 中，在解析 Expr 时候会添加其他复杂信息到 Env，如 common
info enum 等。

### ast 访问

每个 ast 节点访问按如下主要逻辑:

1. update Env。
2. 递归访问 visit 子节点。
3. parse expr, handler 处理, 保证每个节点只会被访问一次。

## TODO

详细规则见: `docs/rule.md` 。
此列表不再更新。

- [x] 已存在变量直接用现有变量名, 不创建新变量名。
- [x] 构造函数中出现的枚举要加到 attr_metas 里, 并且按 if 分支加。
- [] 去掉不用的头文件。
- [x] bs_util.HasXXX 添加 attr_metas 。
- [x] common info 通过 int 来判断, int 是模板参数传进来的。
- [x] 来自中间节点的 common info, 如 live info common info。
- [x] common info case switch。
- [x] action info list, 自定义函数 add_feature, 见 ExtractUserAdItemClickNoInvoke。
- [x] GetCommonInfoAttrNew。
- [x] 多个 action, 用数组保存起来。
